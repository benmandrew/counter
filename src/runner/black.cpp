#include "runner/black.hpp"

#include <poll.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "runner/ltlfilt.hpp"

namespace {

struct ProcessResult {
    int m_exit_code = 0;
    std::string m_output;
    bool m_timed_out = false;
    double m_cpu_s = 0.0;
};

double rusage_cpu_seconds(const struct rusage& usage) {
    const double user_s = static_cast<double>(usage.ru_utime.tv_sec) +
                          (static_cast<double>(usage.ru_utime.tv_usec) / 1e6);
    const double sys_s = static_cast<double>(usage.ru_stime.tv_sec) +
                         (static_cast<double>(usage.ru_stime.tv_usec) / 1e6);
    return user_s + sys_s;
}

// Reads from fd until EOF or deadline, killing child_pid on timeout.
// Returns {output, timed_out}.
std::pair<std::string, bool> read_with_timeout(
    int read_fd, pid_t child_pid,
    std::chrono::steady_clock::time_point deadline) {
    std::string output;
    std::array<char, 4096> read_buf{};
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            kill(child_pid, SIGKILL);
            return {output, true};
        }
        const auto remaining_ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                  now)
                .count();
        const int poll_ms = remaining_ms > std::numeric_limits<int>::max()
                                ? std::numeric_limits<int>::max()
                                : static_cast<int>(remaining_ms);
        struct pollfd pfd{};
        pfd.fd = read_fd;
        pfd.events = POLLIN;
        const int poll_ret = poll(&pfd, 1, poll_ms);
        if (poll_ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            assert(false);
            __builtin_unreachable();
        }
        if (poll_ret == 0) {
            kill(child_pid, SIGKILL);
            return {output, true};
        }
        const ssize_t bytes_read =
            read(read_fd, read_buf.data(), read_buf.size());
        if (bytes_read > 0) {
            output.append(read_buf.data(),
                          static_cast<std::size_t>(bytes_read));
            continue;
        }
        if (bytes_read == 0) {
            return {output, false};
        }
        if (errno == EINTR) {
            continue;
        }
        assert(false);
        __builtin_unreachable();
    }
}

ProcessResult execute_and_capture(const std::vector<std::string>& arguments,
                                  std::chrono::milliseconds timeout) {
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
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    auto [output, timed_out] =
        read_with_timeout(pipe_fds[0], child_pid, deadline);
    close(pipe_fds[0]);
    int wait_status = 0;
    struct rusage child_usage{};
    [[maybe_unused]] const pid_t waited =
        wait4(child_pid, &wait_status, 0, &child_usage);
    assert(waited >= 0);
    int exit_code = -1;
    if (WIFEXITED(wait_status)) {
        exit_code = WEXITSTATUS(wait_status);
    } else if (WIFSIGNALED(wait_status)) {
        exit_code = 128 + WTERMSIG(wait_status);
    }
    return {exit_code, std::move(output), timed_out,
            rusage_cpu_seconds(child_usage)};
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

std::optional<bool> SatisfiabilityChecker::check_satisfiability(
    const std::string& ltl_formula) {
    const std::string normalised = simplify_ltl(ltl_formula);
    // A formula that SPOT reduces to a boolean constant is already decided:
    // "0" is unsatisfiable, "1" is valid and therefore satisfiable. The
    // genetic algorithm generates these constantly — mostly implication checks
    // that reduce away entirely — and they are the bulk of what black would
    // otherwise time out on. black cannot be asked these directly either: it
    // parses SPOT's "0"/"1" as a syntax error and this codebase's
    // "true"/"false" atoms as free variables, so it answers SAT for both.
    if (normalised == "0") {
        n_constant_folded++;
        return false;
    }
    if (normalised == "1") {
        n_constant_folded++;
        return true;
    }
    {
        std::shared_lock lock(m_cache_mutex);
        const auto found = m_cache.find(normalised);
        if (found != m_cache.end()) {
            n_cache_hits++;
            return found->second;
        }
    }
    n_cache_misses++;
    const std::string black = black_executable_path();
    assert(access(black.c_str(), F_OK) == 0);
    const auto timeout_s =
        std::chrono::duration_cast<std::chrono::seconds>(m_timeout).count();
    // Pass ltl_formula (not normalised) to black. black does parse SPOT's
    // compact operator notation ("GFa", "a W b"), but not every token SPOT can
    // emit: "0"/"1" are a syntax error and "xor" is unsupported. The constant
    // cases are handled above; the rest stay on the original formula, which is
    // always black-compatible because it comes from requirement_to_ltl /
    // implication check construction. The normalised form is the cache key.
    const std::vector<std::string> command = {
        black, "solve", "-t", std::to_string(timeout_s), "-f", ltl_formula};
    const auto start = std::chrono::steady_clock::now();
    const ProcessResult result = execute_and_capture(command, m_timeout);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    std::scoped_lock lock(m_cache_mutex);
    total_time_s += elapsed;
    total_cpu_s += result.m_cpu_s;
    if (result.m_timed_out) {
        n_timeouts++;
        m_cache.emplace(normalised, std::nullopt);
        return std::nullopt;
    }
    // Check UNSAT before SAT: the former contains the latter as a substring.
    bool sat = false;
    if (result.m_output.find("UNSAT") != std::string::npos) {
        sat = false;
    } else if (result.m_output.find("SAT") != std::string::npos) {
        sat = true;
    } else if (result.m_output.find("UNKNOWN") != std::string::npos) {
        // black's own internal timeout fired before our outer deadline: it
        // exited normally with "UNKNOWN (stopped at k = N)".  Treat as
        // indeterminate, same as a process-level timeout.
        n_timeouts++;
        m_cache.emplace(normalised, std::nullopt);
        return std::nullopt;
    } else {
        // black's output crossed a process boundary and didn't match any
        // expected form: don't let assert() (a no-op in release builds) fall
        // through to __builtin_unreachable(), which is undefined behavior if
        // this branch is ever actually taken.
        throw std::runtime_error("unexpected output from black: " +
                                 result.m_output);
    }
    m_cache.emplace(normalised, sat);
    return sat;
}
