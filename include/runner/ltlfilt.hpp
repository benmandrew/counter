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
