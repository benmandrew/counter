#include "runner/spot.hpp"

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
    int m_exit_code;
    std::string m_output;
};

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

int wait_for_child(pid_t child_pid) {
    int wait_status = 0;
    [[maybe_unused]] const pid_t waited = waitpid(child_pid, &wait_status, 0);
    assert(waited >= 0);
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
    int exit_code = wait_for_child(child_pid);
    return {exit_code, output};
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
    const std::string normalised = normalize_ltl(formula);
    static std::unordered_map<std::string, std::string> cache;
    static std::mutex cache_mutex;
    {
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(normalised);
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
        execute_and_capture({binary, "-D", "-S", "-H", "-f", normalised});
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    if (result.m_exit_code != 0) {
        // A non-zero exit here (e.g. a subprocess spawn failure under heavy
        // concurrent forking) must not be cached as a successful result
        throw std::runtime_error("ltl2tgba exited with code " +
                                 std::to_string(result.m_exit_code) +
                                 " for formula: " + normalised);
    }
    std::scoped_lock lock(cache_mutex);
    Ltl2tgbaStats::total_time_s += elapsed;
    cache.emplace(normalised, result.m_output);
    return result.m_output;
}

bool RealizabilityChecker::check_realizability(
    const Specification& specification) {
    std::string conj_ltl;
    build_specification_formula(specification, conj_ltl);
    conj_ltl = normalize_ltl(conj_ltl);
    const std::string cache_key = conj_ltl + "|" +
                                  join_comma(specification.m_in_atoms) + "|" +
                                  join_comma(specification.m_out_atoms);
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
    if (!specification.m_in_atoms.empty()) {
        command.push_back("--ins=" + join_comma(specification.m_in_atoms));
    } else if (!specification.m_out_atoms.empty()) {
        command.push_back("--outs=" + join_comma(specification.m_out_atoms));
    }
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = execute_and_capture(command);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    const bool realizable = parse_realizability_output(result);
    std::scoped_lock lock(m_cache_mutex);
    total_time_s += elapsed;
    m_cache.emplace(cache_key, realizable);
    return realizable;
}
