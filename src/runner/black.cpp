#include "runner/black.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct ProcessResult {
    int m_exit_code;
    std::string m_output;
};

ProcessResult execute_and_capture(const std::vector<std::string>& arguments) {
    assert(!arguments.empty());
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
        std::vector<char*> argv(arguments.size() + 1);
        for (std::size_t arg_idx = 0; arg_idx < arguments.size(); ++arg_idx) {
            argv[arg_idx] = const_cast<char*>(arguments[arg_idx].c_str());
        }
        argv[arguments.size()] = nullptr;
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

}  // namespace

SatisfiabilityChecker& global_sat_checker() {
    static SatisfiabilityChecker instance;
    return instance;
}

std::string black_executable_path() {
#ifdef BLACK_EXECUTABLE_PATH
    return BLACK_EXECUTABLE_PATH;
#else
    assert(false);
    return "";
#endif
}

bool SatisfiabilityChecker::check_satisfiability(
    const std::string& ltl_formula) {
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        const auto found = m_cache.find(ltl_formula);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
        n_cache_misses++;
    }
    const std::string black = black_executable_path();
    assert(access(black.c_str(), F_OK) == 0);
    const std::vector<std::string> command = {black, "solve", "-f",
                                              ltl_formula};
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = execute_and_capture(command);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    // Check UNSAT before SAT: the former contains the latter as a substring.
    bool sat = false;
    if (result.m_output.find("UNSAT") != std::string::npos) {
        sat = false;
    } else if (result.m_output.find("SAT") != std::string::npos) {
        sat = true;
    } else {
        assert(false);
    }
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    total_time_s += elapsed;
    m_cache.emplace(ltl_formula, sat);
    return sat;
}
