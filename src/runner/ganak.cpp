#include "runner/ganak.hpp"

#include <unistd.h>

#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "formula_key.hpp"
#include "prop_formula.hpp"
#include "runner/ltlfilt.hpp"
#include "runner/process.hpp"
#include "tool_paths.hpp"

namespace {

// Removes the temporary DIMACS file however the enclosing scope exits.
// run_ganak_on_dimacs throws on a non-zero exit, which otherwise leaks the
// file into the system temp directory for every failed count.
class TempFileGuard {
   public:
    explicit TempFileGuard(std::string path) : m_path(std::move(path)) {}
    ~TempFileGuard() { std::remove(m_path.c_str()); }
    TempFileGuard(const TempFileGuard&) = delete;
    TempFileGuard& operator=(const TempFileGuard&) = delete;
    TempFileGuard(TempFileGuard&&) = delete;
    TempFileGuard& operator=(TempFileGuard&&) = delete;

   private:
    std::string m_path;
};

std::string temp_directory() {
    try {
        return std::filesystem::temp_directory_path().string();
    } catch (const std::filesystem::filesystem_error&) {
        // Keep a deterministic fallback when no system temp directory resolves.
        return "/tmp";
    }
}

std::string write_temporary_dimacs(const std::string& contents) {
    std::string writable_template =
        temp_directory() + "/counter-formula-XXXXXX";
    std::vector<char> buffer(writable_template.begin(),
                             writable_template.end());
    buffer.push_back('\0');
    const int file_descriptor = mkstemp(buffer.data());
    assert(file_descriptor >= 0);
    close(file_descriptor);
    std::string dimacs_path(buffer.data());
    std::ofstream dimacs_file(dimacs_path);
    assert(dimacs_file.good());
    dimacs_file << contents;
    dimacs_file.close();
    assert(static_cast<bool>(dimacs_file));
    return dimacs_path;
}

Count parse_ganak_exact_count(const std::string& output) {
    const std::string prefix = "c s exact arb int ";
    const std::size_t prefix_position = output.find(prefix);
    if (prefix_position != std::string::npos) {
        const std::size_t number_start = prefix_position + prefix.size();
        std::size_t number_end = number_start;
        while (number_end < output.size() &&
               (std::isdigit(static_cast<unsigned char>(output[number_end])) !=
                0)) {
            ++number_end;
        }
        if (number_end > number_start) {
            const std::string number_text =
                output.substr(number_start, number_end - number_start);
            return parse_count_decimal_or_throw(number_text);
        }
    }
    if (output.find("s UNSATISFIABLE") != std::string::npos) {
        return 0;
    }
    // ganak's output crossed a process boundary and didn't match either
    // expected form: don't let assert() (a no-op in release builds) treat
    // this as success and cache a fabricated count of 0.
    throw std::runtime_error("unrecognized ganak output: " + output);
}

}  // namespace

std::string ganak_executable_path() {
#ifdef GANAK_EXECUTABLE_PATH
    static const ToolPath k_path =
        tool_path_from_env("COUNTER_GANAK_PATH", GANAK_EXECUTABLE_PATH);
    return k_path.m_path;
#else
    assert(false);
    return "";
#endif
}

Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed,
                          double* cpu_s_out) {
    assert(access(dimacs_path.c_str(), F_OK) == 0);
    const std::string ganak_path = ganak_executable_path();
    assert(access(ganak_path.c_str(), F_OK) == 0);
    const std::vector<std::string> command = {
        ganak_path,
        "--seed",
        std::to_string(seed),
        dimacs_path,
    };
    // Untimed: counting is the fitness function's real work, so a slow count
    // is usually a legitimately hard one rather than a blowup.
    const ProcessResult result =
        execute_and_capture(command, std::chrono::milliseconds::zero());
    if (cpu_s_out != nullptr) {
        *cpu_s_out = result.m_cpu_s;
    }
    if (result.m_exit_code != 0) {
        throw std::runtime_error("ganak exited with code " +
                                 std::to_string(result.m_exit_code));
    }
    return parse_ganak_exact_count(result.m_output);
}

Count run_ganak_on_formula(const std::string& formula, unsigned seed) {
    const std::string normalised = normalize_ltl(formula);
    static std::unordered_map<std::string, Count> cache;
    static std::mutex cache_mutex;
    // Keyed on the canonical renamed form rather than on the caller's
    // spelling. A model count is invariant under a bijection on the atoms --
    // ganak counts over the variables the formula mentions, and the free
    // variables are multiplied back in by count_guard_models outside this
    // cache -- so two guards differing only in operand order, association or
    // atom naming share one exec. Measured over nine specifications that is
    // 20.3% to 49.5% of the execs a run makes.
    const std::string key =
        formula_key::renamed(normalised) + "|" + std::to_string(seed);
    {
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(key);
        if (found != cache.end()) {
            GanakStats::n_cache_hits++;
            return found->second;
        }
        GanakStats::n_cache_misses++;
    }
    const Formula parsed = Formula(normalised);
    const std::string formula_dimacs_path =
        write_temporary_dimacs(parsed.to_dimacs());
    const TempFileGuard dimacs_guard(formula_dimacs_path);
    const auto start = std::chrono::steady_clock::now();
    const auto elapsed_since_start = [&start] {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             start)
            .count();
    };
    double cpu_s = 0.0;
    const Count count = run_ganak_on_dimacs(formula_dimacs_path, seed, &cpu_s);
    const double elapsed = elapsed_since_start();
    std::scoped_lock lock(cache_mutex);
    GanakStats::total_time_s += elapsed;
    GanakStats::total_cpu_s += cpu_s;
    cache.emplace(key, count);
    return count;
}
