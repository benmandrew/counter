#pragma once

/// @file ltlfilt.hpp
/// @brief Wrapper for ltlfilt (SPOT) that normalises LTL formulae to a
///        canonical form, improving cache hit rates across tool invocations.

#include <string>

struct LtlfiltStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
};

/// Returns the full filesystem path to the ltlfilt binary.
std::string ltlfilt_path();

/// Returns the ltlfilt-simplified canonical form of `formula`. The result is
/// memoised: the subprocess is launched at most once per unique input string.
/// Returns `formula` unchanged if the binary is inaccessible or exits
/// non-zero.
std::string normalize_ltl(const std::string& formula);

/// Returns true if `lhs` and `rhs` are logically equivalent LTL formulae,
/// checked via `ltlfilt --equivalent-to`. This is a best-effort
/// cross-validation helper (e.g. for differential-testing the hand-rolled
/// LTL translator against another source of truth), not a correctness
/// boundary: if the binary is inaccessible or the check is otherwise
/// inconclusive (any exit status other than ltlfilt's own match/no-match
/// codes 0/1), it returns true rather than reporting a false mismatch.
bool ltl_equivalent(const std::string& lhs, const std::string& rhs);
