#pragma once

/// @file process.hpp
/// @brief The fork/exec wrapper behind every external tool runner (ltl2tgba,
///        ltlsynt, black, ltlfilt, ganak, node), holding the timeout and
///        orphan-containment policy in one place.

#include <sys/types.h>  // NOLINT(build/include_order) — pid_t

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

/// Outcome of one external tool invocation.
struct ProcessResult {
    /// The child's exit status, or 128 + signal number if a signal killed it
    /// (including the SIGKILL this wrapper sends on timeout).
    int m_exit_code = -1;
    /// Merged stdout and stderr, as far as it was read before EOF or timeout.
    std::string m_output;
    /// The child's user+sys CPU seconds, from wait4's rusage.
    double m_cpu_s = 0.0;
    /// True if the deadline expired: the output is partial and the process
    /// group was killed.
    bool m_timed_out = false;
};

/// How to resolve arguments[0] to a binary.
enum class ExecutableLookup : std::uint8_t {
    /// execv, for the full paths CMake resolves for the tools it fetches or
    /// builds (ltl2tgba, ltlsynt, ltlfilt, black, ganak).
    AbsolutePath,
    /// execvp, for a general system interpreter found on PATH (node).
    SearchPath,
};

/// Runs `arguments` with stdout and stderr merged into the returned output,
/// reaping the child before returning.
///
/// A non-zero `timeout` is a wall-clock budget over the whole call, covering
/// both waiting for output and reaping. On expiry the child's process group is
/// SIGKILLed and `m_timed_out` is set. Zero means no deadline, which is the
/// only way this function can block indefinitely.
ProcessResult execute_and_capture(
    const std::vector<std::string>& arguments,
    std::chrono::milliseconds timeout = std::chrono::milliseconds::zero(),
    ExecutableLookup lookup = ExecutableLookup::AbsolutePath);

/// Whether the child should be killed when the thread that forked it goes
/// away. PR_SET_PDEATHSIG is tied to the forking *thread*, not to the process,
/// which makes it safe for a call that forks and waits in one place and wrong
/// for anything longer-lived.
enum class ParentDeathPolicy : std::uint8_t {
    /// Set PR_SET_PDEATHSIG. Correct only when the forking thread waits for
    /// the child before returning, as execute_and_capture does.
    KillWithParentThread,
    /// Leave PDEATHSIG unset, for a child that outlives the thread that
    /// spawned it. Setting it here would kill the child as soon as that thread
    /// returned — for a lazily-spawned persistent process that means the first
    /// pool worker to touch it. Containment on parent death is then the
    /// owner's teardown to arrange.
    SurviveParentThread,
};

/// Applies the child half of the containment policy: puts the child in its own
/// process group, and applies `policy`. Call only between fork() and exec(),
/// where it must stay async-signal-safe; under KillWithParentThread it _exit()s
/// if the parent is already gone. execute_and_capture calls this itself — it is
/// exposed for PersistentProcess, which owns its own fork.
///
/// Note that the new process group is not the terminal's foreground group, so
/// a Ctrl-C no longer reaches the tool directly. Under KillWithParentThread the
/// PDEATHSIG half covers that: the signal kills this process, and the kernel
/// then kills the child.
void harden_child_after_fork(ParentDeathPolicy policy);

/// The parent half of the same policy, called immediately after fork(): repeats
/// the child's setpgid so the group exists no matter which side is scheduled
/// first. Without it a timeout firing before the child was scheduled would
/// killpg a group that does not exist yet, and the tool would survive.
void adopt_child_process_group(pid_t child_pid);

/// SIGKILLs the process group led by `pid`, then `pid` itself. Must be called
/// before the child is reaped, while the group still has a member and the pid
/// cannot have been reused.
void kill_process_tree(pid_t pid);

/// Reaps `pid`, giving it `grace` to exit on its own before killing its process
/// group. Returns the child's user+sys CPU seconds. For a child that is asked
/// to shut down cleanly first, such as the formaliser on stdin EOF.
double reap_with_grace(pid_t pid, std::chrono::milliseconds grace);
