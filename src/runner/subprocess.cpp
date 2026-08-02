#include "runner/subprocess.hpp"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "profile.hpp"

namespace {

double rusage_cpu_seconds(const struct rusage& usage) {
    const double user_s = static_cast<double>(usage.ru_utime.tv_sec) +
                          (static_cast<double>(usage.ru_utime.tv_usec) / 1e6);
    const double sys_s = static_cast<double>(usage.ru_stime.tv_sec) +
                         (static_cast<double>(usage.ru_stime.tv_usec) / 1e6);
    return user_s + sys_s;
}

// The child half of the fork() path, taken only when the caller asked for
// PR_SET_PDEATHSIG. Everything here must be async-signal-safe: no allocation,
// which is why argv is built before the fork.
[[noreturn]] void child_exec(char* const* argv, const char* executable,
                             int write_fd) {
    if (dup2(write_fd, STDOUT_FILENO) < 0 ||
        dup2(write_fd, STDERR_FILENO) < 0) {
        _exit(127);
    }
    close(write_fd);
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    // Closes the race where the parent died between the fork and the request
    // above, which would leave the request registered against a parent that is
    // already gone.
    if (getppid() == 1) {
        _exit(127);
    }
    execv(executable, argv);
    _exit(127);
}

bool spawn_child(char* const* argv, const char* executable,
                 const std::array<int, 2>& pipe_fds, bool die_with_parent,
                 pid_t& child_pid) {
    COUNTER_PROFILE_SCOPE("proc/fork+exec");
    if (die_with_parent) {
        // fork(), not posix_spawn: there is no spawn attribute for
        // PR_SET_PDEATHSIG, and losing the orphan protection would be the worse
        // trade for the two tools that ask for it -- they are the multi-GB
        // ones.
        child_pid = fork();
        if (child_pid < 0) {
            return false;
        }
        if (child_pid == 0) {
            close(pipe_fds[0]);
            child_exec(argv, executable, pipe_fds[1]);
        }
        return true;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[0]);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_adddup2(&actions, pipe_fds[1], STDERR_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipe_fds[1]);
    const int spawn_result =
        posix_spawn(&child_pid, executable, &actions, nullptr, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    return spawn_result == 0;
}

// Drains read_fd until EOF, or until the deadline if there is one. Returns the
// bytes read so far and whether the deadline expired.
std::pair<std::string, bool> read_until_eof(int read_fd,
                                            std::chrono::milliseconds timeout) {
    COUNTER_PROFILE_SCOPE("proc/read");
    const bool timed = timeout > std::chrono::milliseconds::zero();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::string output;
    std::array<char, 4096> buf{};
    while (true) {
        if (timed) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now())
                    .count();
            if (remaining <= 0) {
                return {output, true};
            }
            const int poll_ms = remaining > std::numeric_limits<int>::max()
                                    ? std::numeric_limits<int>::max()
                                    : static_cast<int>(remaining);
            struct pollfd pfd{read_fd, POLLIN, 0};
            const int ready = poll(&pfd, 1, poll_ms);
            if (ready < 0) {
                if (errno == EINTR) {
                    continue;
                }
                assert(false);
                __builtin_unreachable();
            }
            if (ready == 0) {
                return {output, true};
            }
        }
        const ssize_t bytes_read =
            read(  // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
                read_fd, buf.data(), buf.size());
        if (bytes_read > 0) {
            output.append(buf.data(), static_cast<std::size_t>(bytes_read));
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

int reap(pid_t child_pid, double& cpu_s_out) {
    COUNTER_PROFILE_SCOPE("proc/wait");
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

}  // namespace

SubprocessResult run_subprocess(const std::vector<std::string>& arguments,
                                const SubprocessOptions& options) {
    assert(!arguments.empty());
    // argv is built before spawning: on the fork() path, allocating in the
    // child between fork and exec deadlocks if another thread happened to hold
    // the allocator lock at the moment of the fork.
    std::vector<char*> argv(arguments.size() + 1);
    for (std::size_t arg_idx = 0; arg_idx < arguments.size(); ++arg_idx) {
        argv[arg_idx] = const_cast<char*>(arguments[arg_idx].c_str());
    }
    argv[arguments.size()] = nullptr;

    std::array<int, 2> pipe_fds = {-1, -1};
    [[maybe_unused]] const int pipe_result = pipe2(pipe_fds.data(), O_CLOEXEC);
    assert(pipe_result == 0);

    pid_t child_pid = -1;
    if (!spawn_child(argv.data(), arguments[0].c_str(), pipe_fds,
                     options.m_die_with_parent, child_pid)) {
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        assert(false);
        __builtin_unreachable();
    }
    close(pipe_fds[1]);

    auto [output, timed_out] = read_until_eof(pipe_fds[0], options.m_timeout);
    if (timed_out) {
        // Kill before reaping, or the wait below would block on exactly the
        // query the deadline exists to abandon.
        kill(child_pid, SIGKILL);
    }
    close(pipe_fds[0]);

    double cpu_s = 0.0;
    const int exit_code = reap(child_pid, cpu_s);
    return {exit_code, std::move(output), cpu_s, timed_out};
}
