#include "runner/ganak.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "prop_formula.hpp"
#include "runner/ltlfilt.hpp"

namespace {

struct ProcessResult {
    int m_exit_code = 0;
    std::string m_output;
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

ProcessResult execute_and_capture(const std::vector<std::string>& arguments) {
    assert(!arguments.empty());
    // Build argv before forking: heap allocation inside the child between
    // fork() and execv() can deadlock if another thread held the allocator
    // lock at the moment of the fork (e.g. under ASAN's allocator).
    std::vector<char*> argv(arguments.size() + 1);
    for (std::size_t arg_idx = 0; arg_idx < arguments.size(); ++arg_idx) {
        argv[arg_idx] = const_cast<char*>(arguments[arg_idx].c_str());
    }
    argv[arguments.size()] = nullptr;
    std::array<int, 2> pipe_fds = {-1, -1};
    [[maybe_unused]] const int pipe_result = pipe(pipe_fds.data());
    assert(pipe_result == 0);
    const pid_t child_pid = fork();
    if (child_pid < 0) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        assert(false);
        __builtin_unreachable();
    }
    if (child_pid == 0) {
        close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0 ||
            dup2(pipe_fds[1], STDERR_FILENO) < 0) {
            _exit(127);
        }
        close(pipe_fds[1]);
        execv(arguments[0].c_str(), argv.data());
        _exit(127);
    }
    close(pipe_fds[1]);
    std::string output;
    std::array<char, 4096> read_buf{};
    while (true) {
        const ssize_t bytes_read =
            read(pipe_fds[0], read_buf.data(), read_buf.size());
        if (bytes_read > 0) {
            output.append(read_buf.data(),
                          static_cast<std::size_t>(bytes_read));
            continue;
        }
        if (bytes_read == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        close(pipe_fds[0]);
        assert(false);
        __builtin_unreachable();
    }
    close(pipe_fds[0]);
    int wait_status = 0;
    [[maybe_unused]] const pid_t waited = waitpid(child_pid, &wait_status, 0);
    assert(waited >= 0);
    int exit_code = -1;
    if (WIFEXITED(wait_status)) {
        exit_code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        exit_code = 128 + WTERMSIG(wait_status);
    }
    return {exit_code, output};
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
    return GANAK_EXECUTABLE_PATH;
#else
    assert(false);
    return "";
#endif
}

Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed) {
    assert(access(dimacs_path.c_str(), F_OK) == 0);
    const std::string ganak_path = ganak_executable_path();
    assert(access(ganak_path.c_str(), F_OK) == 0);
    const std::vector<std::string> command = {
        ganak_path,
        "--seed",
        std::to_string(seed),
        dimacs_path,
    };
    const ProcessResult result = execute_and_capture(command);
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
    const std::string key = normalised + "|" + std::to_string(seed);
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
    const auto start = std::chrono::steady_clock::now();
    const Count count = run_ganak_on_dimacs(formula_dimacs_path, seed);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::remove(formula_dimacs_path.c_str());
    std::scoped_lock lock(cache_mutex);
    GanakStats::total_time_s += elapsed;
    cache.emplace(key, count);
    return count;
}
