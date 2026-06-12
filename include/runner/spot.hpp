#pragma once

#include <string>
#include <unordered_map>

#include "requirement.hpp"

/// Returns the directory containing SPOT tool binaries, set at build time via
/// the SPOT_BIN_DIR preprocessor definition.
std::string spot_bin_dir();

/// Returns the full filesystem path to the ltlsynt binary.
std::string ltlsynt_path();

/// Returns the full filesystem path to the ltl2tgba binary.
std::string ltl2tgba_path();

/// Runs ltl2tgba with -D (deterministic), -S (state-based acceptance), and
/// -H (HOA output) on the given LTL formula and returns the raw HOA text.
/// Asserts that the binary is accessible and that the process exits cleanly.
std::string run_ltl2tgba_for_counting(const std::string& formula);

struct Ltl2tgbaStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
};

class RealizabilityChecker {
   public:
    inline static std::size_t n_cache_misses = 0;
    inline static std::size_t n_cache_hits = 0;
    inline static double total_time_s = 0.0;

    /// Checks whether the specification is realizable using ltlsynt. Results
    /// are memoised by the full specification formula, so repeated calls with
    /// identical inputs incur no additional tool invocations.
    bool check_realizability(const Specification& specification);

   private:
    std::unordered_map<std::string, bool> m_cache;
};
