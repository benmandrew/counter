#pragma once

/// @file subprocess.hpp
/// @brief One implementation of "run a tool, capture its output, reap it",
///        shared by every external-tool wrapper.
///
/// Each runner used to carry its own near-identical copy of this. That is not a
/// performance problem in itself, but it means every fix to the spawn path has
/// to be made once per copy, and the two made recently -- moving to
/// posix_spawn, and opening the pipes O_CLOEXEC -- each had to be repeated
/// across all of them. The copies differed only in whether they supported a
/// timeout and whether the child needed to die with its parent, so both are
/// options here rather than reasons to fork the code.

#include <chrono>
#include <string>
#include <utility>
#include <vector>

struct SubprocessResult {
    int m_exit_code = 0;
    std::string m_output;
    /// Child user+sys CPU from wait4(), which unlike wall time excludes the
    /// parent's time blocked waiting.
    double m_cpu_s = 0.0;
    /// True when the deadline expired and the child was killed. The output
    /// collected before that point is still returned.
    bool m_timed_out = false;
};

struct SubprocessOptions {
    /// Wall-clock budget for the whole call. Zero (the default) never expires.
    std::chrono::milliseconds m_timeout{0};
    /// Deliver SIGKILL to the child if this process dies, so a killed run
    /// cannot leave an orphan behind. Only ltlsynt and ltl2tgba need it, and
    /// they pay for it: there is no posix_spawn attribute for
    /// prctl(PR_SET_PDEATHSIG), so requesting it forces the more expensive
    /// fork() path.
    bool m_die_with_parent = false;
};

/// Runs @p arguments (argv[0] is the executable path), capturing stdout and
/// stderr together, and reaps the child before returning.
///
/// Spawned with posix_spawn unless @p options asks for m_die_with_parent.
/// glibc implements posix_spawn with clone(CLONE_VM|CLONE_VFORK), which copies
/// no page tables and does not write-protect the parent -- which matters here
/// because the scoring pool spawns from many threads while the others keep
/// writing. The pipe is O_CLOEXEC so a child cannot inherit, and hold open, a
/// pipe belonging to another call still in flight; a long-lived child that did
/// so would stop that call's reader ever seeing EOF.
SubprocessResult run_subprocess(const std::vector<std::string>& arguments,
                                const SubprocessOptions& options = {});

/// Drains @p read_fd until EOF, or until @p timeout expires; zero never
/// expires. Returns the bytes read so far and whether the deadline expired.
///
/// Exposed for run_ltlfilt_batch, which drives a bidirectional pipe pair that
/// run_subprocess does not cover and so spawns for itself. Its read loop needs
/// the same deadline handling, and a second copy of it is exactly the
/// duplication this file exists to remove. Killing the child on expiry is the
/// caller's job here, since only the caller knows the pid.
std::pair<std::string, bool> read_until_eof(int read_fd,
                                            std::chrono::milliseconds timeout);
