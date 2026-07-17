#pragma once

/// @file spot.hpp
/// @brief Wrappers for SPOT tools: ltl2tgba (automaton construction) and
///        ltlsynt (realizability checking), with memoising checker classes.

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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
    // Child-process CPU time (user+sys), from wait4(); unlike total_time_s
    // (wall) it excludes time the parent spends blocked waiting on the child.
    inline static double total_cpu_s = 0.0;
    // ltl2tgba exit-2-on-tautology results substituted with the universal
    // automaton (see run_ltl2tgba_for_counting) rather than raised as errors.
    inline static std::size_t n_tautology_substitutions = 0;
};

class RealizabilityChecker {
   public:
    inline static std::size_t n_cache_misses = 0;
    inline static std::size_t n_cache_hits = 0;
    inline static double total_time_s = 0.0;
    inline static double total_cpu_s = 0.0;

    /// Checks whether the specification is realizable using ltlsynt. Results
    /// are memoised by the full specification formula, so repeated calls with
    /// identical inputs incur no additional tool invocations.
    bool check_realizability(const Specification& specification);

    /// Realizability of a raw LTL formula with the given input/output atom
    /// partition (mode-agnostic core shared by the FRETISH and TLSF front
    /// ends). Memoised by (normalised formula, inputs, outputs).
    bool check_realizability_ltl(const std::string& ltl_formula,
                                 const std::vector<std::string>& inputs,
                                 const std::vector<std::string>& outputs);

   private:
    mutable std::mutex m_cache_mutex;
    std::unordered_map<std::string, bool> m_cache;
};

/// Returns the process-lifetime RealizabilityChecker instance. All callers
/// that do not need test isolation should use this instead of constructing
/// their own, so they share the memoisation cache.
RealizabilityChecker& global_real_checker();
