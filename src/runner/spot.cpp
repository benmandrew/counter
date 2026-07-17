#include "runner/spot.hpp"

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "requirement.hpp"
#include "runner/ltlfilt.hpp"

namespace {

struct ProcessResult {
    int m_exit_code = 0;
    std::string m_output;
    double m_cpu_s = 0.0;
};

double rusage_cpu_seconds(const struct rusage& usage) {
    const double user_s = static_cast<double>(usage.ru_utime.tv_sec) +
                          (static_cast<double>(usage.ru_utime.tv_usec) / 1e6);
    const double sys_s = static_cast<double>(usage.ru_stime.tv_sec) +
                         (static_cast<double>(usage.ru_stime.tv_usec) / 1e6);
    return user_s + sys_s;
}

void spawn_child_and_exec(char* const* argv, const char* executable,
                          int write_fd) {
    if (dup2(write_fd, STDOUT_FILENO) < 0 ||
        dup2(write_fd, STDERR_FILENO) < 0) {
        _exit(127);
    }
    close(write_fd);
    execv(executable, argv);
    _exit(127);
}

std::string read_from_fd(int read_fd) {
    std::string output;
    std::array<char, 4096> read_buf{};
    while (true) {
        const ssize_t bytes_read =
            read(read_fd, read_buf.data(), read_buf.size());
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
        assert(false);
        __builtin_unreachable();
    }
    return output;
}

int wait_for_child(pid_t child_pid, double& cpu_s_out) {
    int wait_status = 0;
    struct rusage child_usage{};
    [[maybe_unused]] const pid_t waited =
        wait4(child_pid, &wait_status, 0, &child_usage);
    assert(waited >= 0);
    cpu_s_out = rusage_cpu_seconds(child_usage);
    if (WIFEXITED(wait_status)) {
        return WEXITSTATUS(wait_status);
    }
    if (WIFSIGNALED(wait_status)) {
        return 128 + WTERMSIG(wait_status);
    }
    return -1;
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
        spawn_child_and_exec(argv.data(), arguments[0].c_str(), pipe_fds[1]);
    }
    close(pipe_fds[1]);
    std::string output = read_from_fd(pipe_fds[0]);
    close(pipe_fds[0]);
    double cpu_s = 0.0;
    int exit_code = wait_for_child(child_pid, cpu_s);
    return {exit_code, output, cpu_s};
}

std::string join_comma(const std::vector<std::string>& items) {
    std::string result;
    bool first = true;
    for (const auto& item : items) {
        if (!first) {
            result += ',';
        }
        result += item;
        first = false;
    }
    return result;
}

void build_ltl_conjunction(const std::vector<Requirement>& reqs,
                           std::string& out) {
    bool first = true;
    for (const Requirement& req : reqs) {
        if (!first) {
            out += " & ";
        }
        out += "(" + req.m_ltl + ")";
        first = false;
    }
}

void build_specification_formula(const Specification& specification,
                                 std::string& formula) {
    if (specification.m_assumptions.empty()) {
        build_ltl_conjunction(specification.m_guarantees, formula);
        return;
    }
    std::string conj_a;
    build_ltl_conjunction(specification.m_assumptions, conj_a);
    std::string conj_g;
    build_ltl_conjunction(specification.m_guarantees, conj_g);
    formula = "(" + conj_a + ") -> (" + conj_g + ")";
}

// The universal (trivially-true) automaton, in the exact HOA shape ltl2tgba
// itself emits for the constant `true`: one accepting state over zero atoms
// with a `[t]` self-loop. Substituted for the exit-2-on-tautology bug below;
// parses to the same TransferSystem a genuine tautology would.
constexpr const char* k_universal_hoa =
    "HOA: v1\n"
    "name: \"1\"\n"
    "States: 1\n"
    "Start: 0\n"
    "AP: 0\n"
    "acc-name: all\n"
    "Acceptance: 0 t\n"
    "properties: trans-labels explicit-labels state-acc complete\n"
    "properties: deterministic stutter-invariant weak\n"
    "--BODY--\n"
    "State: 0\n"
    "[t] 0\n"
    "--END--\n";

// SPOT 2.15.1's ltl2tgba aborts with exit 2 and this stderr line when a formula
// reduces to a tautology (the printed automaton is universal, hence complete,
// but its prop_complete() flag was left unset). The signature is stable across
// the invocation's binary-path prefix.
bool is_tautology_print_error(const ProcessResult& result) {
    return result.m_exit_code == 2 &&
           result.m_output.find("automaton is complete but prop_complete()") !=
               std::string::npos;
}

bool parse_realizability_output(const ProcessResult& result) {
    if (result.m_output.find("UNREALIZABLE") != std::string::npos) {
        return false;
    }
    if (result.m_output.find("REALIZABLE") != std::string::npos) {
        return true;
    }
    // ltlsynt's output crossed a process boundary and didn't match either
    // expected form: don't let assert() (a no-op in release builds) treat
    // this as UNREALIZABLE and cache a fabricated result.
    throw std::runtime_error("unrecognized ltlsynt output: " + result.m_output);
}

}  // namespace

RealizabilityChecker& global_real_checker() {
    static RealizabilityChecker instance;
    return instance;
}

std::string spot_bin_dir() {
#ifdef SPOT_BIN_DIR
    return SPOT_BIN_DIR;
#else
    assert(false);
    return "";
#endif
}

std::string ltlsynt_path() { return spot_bin_dir() + "/ltlsynt"; }

std::string ltl2tgba_path() { return spot_bin_dir() + "/ltl2tgba"; }

std::string run_ltl2tgba_for_counting(const std::string& formula) {
    // No normalize_ltl() pre-pass here, unlike the other SPOT/black callers.
    // ltl2tgba simplifies internally, so it is redundant -- and ltlfilt
    // --simplify blows up super-exponentially on the deeply nested-X
    // conjunctions this path builds for atom-rich, deep-horizon requirement
    // pairs (e.g. WithinTicks(20) over ~10 atoms): ~1s at 12 ticks, ~21s at
    // 15, unbounded by 20, where ltl2tgba yields the identical automaton from
    // the raw formula in milliseconds. Passing the formula straight through
    // avoids that cliff.
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    {
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(formula);
        if (found != cache.end()) {
            Ltl2tgbaStats::n_cache_hits++;
            return found->second;
        }
        Ltl2tgbaStats::n_cache_misses++;
    }
    const std::string binary = ltl2tgba_path();
    assert(access(binary.c_str(), F_OK) == 0);
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result =
        execute_and_capture({binary, "-D", "-S", "-H", "-f", formula});
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (is_tautology_print_error(result)) {
        // The formula is a tautology: it accepts every trace, so the universal
        // automaton is the correct result, not a scoring failure. Substituting
        // it (rather than letting the throw drop the individual) keeps a
        // genuinely-true formula from counting against the run's
        // max_scoring_failure_rate tolerance.
        std::scoped_lock lock(cache_mutex);
        Ltl2tgbaStats::total_time_s += elapsed;
        Ltl2tgbaStats::total_cpu_s += result.m_cpu_s;
        Ltl2tgbaStats::n_tautology_substitutions++;
        cache.emplace(formula, k_universal_hoa);
        return k_universal_hoa;
    }
    if (result.m_exit_code != 0) {
        // A non-zero exit here (e.g. a subprocess spawn failure under heavy
        // concurrent forking) must not be cached as a successful result
        throw std::runtime_error("ltl2tgba exited with code " +
                                 std::to_string(result.m_exit_code) +
                                 " for formula: " + formula);
    }
    std::scoped_lock lock(cache_mutex);
    Ltl2tgbaStats::total_time_s += elapsed;
    Ltl2tgbaStats::total_cpu_s += result.m_cpu_s;
    cache.emplace(formula, result.m_output);
    return result.m_output;
}

bool RealizabilityChecker::check_realizability(
    const Specification& specification) {
    std::string conj_ltl;
    build_specification_formula(specification, conj_ltl);
    return check_realizability_ltl(conj_ltl, specification.m_in_atoms,
                                   specification.m_out_atoms);
}

bool RealizabilityChecker::check_realizability_ltl(
    const std::string& ltl_formula, const std::vector<std::string>& inputs,
    const std::vector<std::string>& outputs) {
    // No normalize_ltl() pre-pass, matching run_ltl2tgba_for_counting: ltlsynt
    // simplifies internally, and the specification formula is a conjunction of
    // the guarantees, which reproduces the deeply nested-X shape that hangs
    // ltlfilt --simplify for multi-guarantee deep-horizon specs (e.g. two
    // WithinTicks(20) guarantees with different responses -- reachable via
    // mutate_timing, and re-checked here for every survivor). ltlsynt decides
    // the raw formula in milliseconds, so pass it straight through. Unlike the
    // black path, nothing here depends on the "0"/"1" fold normalize enables.
    const std::string& conj_ltl = ltl_formula;
    const std::string cache_key =
        conj_ltl + "|" + join_comma(inputs) + "|" + join_comma(outputs);
    {
        std::scoped_lock lock(m_cache_mutex);
        const auto found = m_cache.find(cache_key);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
        n_cache_misses++;
    }
    const std::string ltlsynt = ltlsynt_path();
    assert(access(ltlsynt.c_str(), F_OK) == 0);
    std::vector<std::string> command = {ltlsynt, "--realizability", "-f",
                                        conj_ltl};
    if (!inputs.empty()) {
        command.push_back("--ins=" + join_comma(inputs));
    } else if (!outputs.empty()) {
        command.push_back("--outs=" + join_comma(outputs));
    }
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = execute_and_capture(command);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    const bool realizable = parse_realizability_output(result);
    std::scoped_lock lock(m_cache_mutex);
    total_time_s += elapsed;
    total_cpu_s += result.m_cpu_s;
    m_cache.emplace(cache_key, realizable);
    return realizable;
}
