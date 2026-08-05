#pragma once

/// @file ltlfilt.hpp
/// @brief Wrapper for ltlfilt (SPOT) that simplifies LTL formulae to a
///        canonical form, improving cache hit rates across tool invocations
///        and deciding formulae that reduce to a boolean constant outright.

#include <chrono>
#include <cstddef>
#include <string>

struct LtlfiltStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
    /// Child-process CPU time (user+sys), from wait4(); unlike total_time_s
    /// (wall) it excludes time the parent spends blocked waiting on the child.
    inline static double total_cpu_s = 0.0;
    /// Calls abandoned at the per-call timeout. Both callers treat these as
    /// inconclusive rather than as errors, so unlike the ltl2tgba and ltlsynt
    /// counters this one costs no individual: it measures how often --simplify
    /// hit its blowup case.
    inline static std::size_t n_timeouts = 0;
};

/// Returns the full filesystem path to the ltlfilt binary.
std::string ltlfilt_path();

/// Per-call wall-clock budget for every ltlfilt exec (process-global, like the
/// ltlsynt and ltl2tgba budgets). A call exceeding it is killed and reported as
/// inconclusive: simplify_ltl returns its input unchanged and ltl_equivalent
/// returns true. Zero disables the timeout. Unlike the other tool budgets this
/// defaults to a non-zero value, because --simplify blows up
/// super-exponentially on deep nested-X conjunctions and losing a
/// simplification costs nothing but a larger formula. Set once at startup from
/// Config::ltlfilt_timeout.
void set_ltlfilt_timeout(std::chrono::milliseconds timeout);

/// Returns the ltlfilt-simplified form of `formula` verbatim, including SPOT's
/// boolean constants "0" (false) and "1" (true) when the formula reduces to
/// one. A constant result settles satisfiability outright, so callers able to
/// act on it can skip a solver entirely; callers that must hand the result to
/// a downstream tool want normalize_ltl instead. The result is memoised: the
/// subprocess is launched at most once per unique input string. Returns
/// `formula` unchanged if the binary is inaccessible or exits non-zero.
std::string simplify_ltl(const std::string& formula);

/// Returns the ltlfilt-simplified canonical form of `formula`. The result is
/// memoised: the subprocess is launched at most once per unique input string.
/// Returns `formula` unchanged if the binary is inaccessible or exits
/// non-zero, or if the formula reduces to a boolean constant (see
/// simplify_ltl) — no single constant spelling is accepted by every downstream
/// tool, so the original formula is returned to keep the result tool-safe.
std::string normalize_ltl(const std::string& formula);

/// Returns true if `lhs` and `rhs` are logically equivalent LTL formulae,
/// checked via `ltlfilt --equivalent-to`. This is a best-effort
/// cross-validation helper (e.g. for differential-testing the hand-rolled
/// LTL translator against another source of truth), not a correctness
/// boundary: if the binary is inaccessible or the check is otherwise
/// inconclusive (any exit status other than ltlfilt's own match/no-match
/// codes 0/1), it returns true rather than reporting a false mismatch.
bool ltl_equivalent(const std::string& lhs, const std::string& rhs);
