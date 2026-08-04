#include "runner/process.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <ctime>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "profile.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double rusage_cpu_seconds(const struct rusage& usage) {
    const double user_s = static_cast<double>(usage.ru_utime.tv_sec) +
                          (static_cast<double>(usage.ru_utime.tv_usec) / 1e6);
    const double sys_s = static_cast<double>(usage.ru_stime.tv_sec) +
                         (static_cast<double>(usage.ru_stime.tv_usec) / 1e6);
    return user_s + sys_s;
}

// Milliseconds left until `deadline`, clamped to poll's int range. -1 (poll
// blocks forever) when there is no deadline, nullopt once it has passed.
std::optional<int> poll_timeout_ms(
    const std::optional<Clock::time_point>& deadline) {
    if (!deadline.has_value()) {
        return -1;
    }
    const auto now = Clock::now();
    if (now >= *deadline) {
        return std::nullopt;
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now)
            .count();
    return remaining > std::numeric_limits<int>::max()
               ? std::numeric_limits<int>::max()
               : static_cast<int>(remaining);
}

// Reads read_fd until the child closes it (EOF) or `deadline` passes, if there
// is one. Returns the bytes read so far and whether the deadline was hit; the
// caller kills and reaps. With no deadline, poll blocks indefinitely and this
// only returns on EOF.
std::pair<std::string, bool> read_until(
    int read_fd, const std::optional<Clock::time_point>& deadline) {
    // The child's own runtime: the parent sits in poll()/read() until the tool
    // closes its stdout. Wall greatly exceeding CPU here is expected and
    // healthy -- it means the parent is blocked, not spinning.
    COUNTER_PROFILE_SCOPE("proc/read");
    std::string output;
    std::array<char, 4096> read_buf{};
    while (true) {
        const std::optional<int> poll_ms = poll_timeout_ms(deadline);
        if (!poll_ms.has_value()) {
            return {output, true};
        }
        struct pollfd pfd{};
        pfd.fd = read_fd;
        pfd.events = POLLIN;
        const int poll_result = poll(&pfd, 1, *poll_ms);
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            assert(false);
            __builtin_unreachable();
        }
        if (poll_result == 0) {
            return {output, true};
        }
        const ssize_t bytes_read =
            read(  // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
                read_fd, read_buf.data(), read_buf.size());
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

// Reaps `pid`, killing its process group and continuing to wait if `deadline`
// passes first. Sets `timed_out` if that kill was needed. Returns the exit
// status in ProcessResult's encoding.
int reap(pid_t pid, const std::optional<Clock::time_point>& deadline,
         double& cpu_s_out, bool& timed_out) {
    // Only the reap, not the kill that may precede it: this is the wait for a
    // child that has already closed its stdout. On the common path the first
    // WNOHANG collects it and the wall time is near zero, so a large total
    // here means tools are lingering after their last write rather than
    // exiting -- which is the failure the deadline on this loop exists to
    // bound.
    COUNTER_PROFILE_SCOPE("proc/wait");
    int wait_status = 0;
    struct rusage child_usage{};
    // Unwrapped once here rather than dereferenced in the loop: the polling
    // branch below is only reachable when there is a deadline, but that is not
    // a fact the optional itself carries.
    const bool bounded = deadline.has_value();
    const Clock::time_point deadline_point =
        deadline.value_or(Clock::time_point::max());
    bool killed = false;
    while (true) {
        // Once the group is killed there is nothing left to wait out, and
        // SIGKILL cannot be caught, so drop the polling and block.
        const int flags = (bounded && !killed) ? WNOHANG : 0;
        const pid_t waited = wait4(pid, &wait_status, flags, &child_usage);
        if (waited == pid) {
            break;
        }
        if (waited < 0) {
            if (errno == EINTR) {
                continue;
            }
            assert(false);
            __builtin_unreachable();
        }
        if (Clock::now() >= deadline_point) {
            kill_process_tree(pid);
            killed = true;
            timed_out = true;
            continue;
        }
        // There is no portable wait-with-timeout, so poll. The common path
        // never sleeps at all: the child is normally already dead by the time
        // its pipe reaches EOF, so the first WNOHANG reaps it. 2 ms is
        // negligible beside the solver calls this wraps.
        const struct timespec nap{0, 2'000'000};
        nanosleep(&nap, nullptr);
    }
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

void harden_child_after_fork(ParentDeathPolicy policy) {
    // Own process group, so one killpg reaches this child *and anything it
    // spawns*. A bare kill(pid) hits only the direct child and strands every
    // grandchild as an orphan reparented to PID 1 — which is how multi-GB
    // ltl2tgba processes have survived past the run that started them.
    setpgid(0, 0);
    if (policy == ParentDeathPolicy::SurviveParentThread) {
        return;
    }
    // Never outlive the parent: if the counter process is killed (a campaign
    // harness enforcing a wall/RAM budget, the OOM killer, Ctrl-C) while this
    // subprocess is mid-run, have the kernel deliver SIGKILL rather than leave
    // the tool running unattended.
    //
    // The kernel ties this to the forking *thread*, not the process, so it is
    // only correct where that thread waits for the child — every exec behind
    // execute_and_capture does. A child that outlives its spawning thread must
    // opt out (see ParentDeathPolicy).
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    // Closes the fork/prctl race: if the parent died in the window before the
    // request was registered, the signal is never coming.
    if (getppid() == 1) {
        _exit(127);
    }
}

void adopt_child_process_group(pid_t child_pid) {
    // The child's own setpgid, repeated here so the group exists whichever
    // side the scheduler runs first — otherwise a timeout firing in that
    // window would killpg a group that does not exist yet. Whichever call
    // loses fails harmlessly (EACCES once the child has exec'd, ESRCH once it
    // has exited), so the result is deliberately ignored.
    [[maybe_unused]] const int setpgid_result = setpgid(child_pid, child_pid);
}

void kill_process_tree(pid_t pid) {
    // The group first, to catch grandchildren; then the pid directly, in case
    // setpgid never took effect and there is no group led by this pid.
    killpg(pid, SIGKILL);
    kill(pid, SIGKILL);
}

double reap_with_grace(pid_t pid, std::chrono::milliseconds grace) {
    double cpu_s = 0.0;
    bool timed_out = false;
    reap(pid, Clock::now() + grace, cpu_s, timed_out);
    return cpu_s;
}

ProcessResult execute_and_capture(const std::vector<std::string>& arguments,
                                  std::chrono::milliseconds timeout,
                                  ExecutableLookup lookup) {
    assert(!arguments.empty());
    // Build argv before forking: heap allocation inside the child between
    // fork() and exec() can deadlock if another thread held the allocator lock
    // at the moment of the fork (e.g. under ASAN's allocator).
    std::vector<char*> argv(arguments.size() + 1);
    for (std::size_t arg_idx = 0; arg_idx < arguments.size(); ++arg_idx) {
        argv[arg_idx] = const_cast<char*>(arguments[arg_idx].c_str());
    }
    argv[arguments.size()] = nullptr;
    std::array<int, 2> pipe_fds = {-1, -1};
    pid_t child_pid = -1;
    {
        // Timed separately from the read and wait below: this scope is the
        // parent's share of process creation -- the pipe, the fork's page
        // table copy, and the group setup -- which is the part a cheaper spawn
        // primitive removes. It is work the parent does itself, so unlike
        // proc/read it should show CPU close to its wall time. The child's own
        // runtime is not in here: the parent leaves this scope as soon as fork
        // returns.
        COUNTER_PROFILE_SCOPE("proc/fork+exec");
        // O_CLOEXEC, because these runners are called from many scoring-pool
        // threads at once. Without it a fork here inherits every pipe another
        // call has open, and holds the write end past its own exec, so that
        // call's reader never sees end of file. pipe2 sets the flag
        // atomically; pipe followed by fcntl would race a concurrent fork. The
        // dup2 onto stdout and stderr below clears the flag on the copies,
        // which is what lets the child keep those two.
        [[maybe_unused]] const int pipe_result =
            pipe2(pipe_fds.data(), O_CLOEXEC);
        assert(pipe_result == 0);
        child_pid = fork();
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
            harden_child_after_fork(ParentDeathPolicy::KillWithParentThread);
            if (lookup == ExecutableLookup::SearchPath) {
                execvp(arguments[0].c_str(), argv.data());
            } else {
                execv(arguments[0].c_str(), argv.data());
            }
            _exit(127);
        }
        adopt_child_process_group(child_pid);
        close(pipe_fds[1]);
    }
    std::optional<Clock::time_point> deadline;
    if (timeout > std::chrono::milliseconds::zero()) {
        deadline = Clock::now() + timeout;
    }
    auto [output, timed_out] = read_until(pipe_fds[0], deadline);
    close(pipe_fds[0]);
    if (timed_out) {
        kill_process_tree(child_pid);
    }
    double cpu_s = 0.0;
    // Reap against the same deadline rather than blocking. EOF on the pipe
    // means the child closed stdout, not that it exited, so a tool that wedges
    // after writing its answer would otherwise outlive its timeout inside
    // wait4 — the one place the old per-runner wrappers could still hang on a
    // call that nominally had a budget.
    const int exit_code =
        reap(child_pid, timed_out ? std::nullopt : deadline, cpu_s, timed_out);
    return {exit_code, std::move(output), cpu_s, timed_out};
}
