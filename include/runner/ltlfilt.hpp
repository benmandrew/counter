#pragma once

/// @file ltlfilt.hpp
/// @brief Wrapper for ltlfilt (SPOT) that simplifies LTL formulae to a
///        canonical form, improving cache hit rates across tool invocations
///        and deciding formulae that reduce to a boolean constant outright.

#include <cstddef>
#include <string>

#include "config.hpp"

struct LtlfiltStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
    // Child-process CPU time (user+sys), from wait4(); unlike total_time_s
    // (wall) it excludes time the parent spends blocked waiting on the child.
    inline static double total_cpu_s = 0.0;
};

/// Returns the full filesystem path to the ltlfilt binary.
std::string ltlfilt_path();

/// Sets how many ltlfilt batches may run concurrently, from
/// Config::ltlfilt_batchers. Concurrent simplify_ltl misses are coalesced into
/// one exec per batcher, trading latency for CPU; zero disables batching and
/// gives every call its own exec. Call once at startup, before any scoring
/// thread runs: the pool is built on first use and not resized afterwards.
void set_ltlfilt_batchers(std::size_t count);

/// Selects where simplify_ltl does its work, from Config::simplify_engine.
/// Under the default SimplifyEngine::Libspot nothing below reaches ltlfilt at
/// all and set_ltlfilt_batchers has no effect. Call once at startup, for the
/// same reason.
void set_simplify_engine(SimplifyEngine engine);

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
