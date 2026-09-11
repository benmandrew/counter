#pragma once

/// @file ganak.hpp
/// @brief Wrapper around the Ganak weighted model counter, used to count
///        satisfying valuations for automaton transitions.

#include <cstddef>
#include <stdexcept>
#include <string>

#include "fitness/transfer_matrix.hpp"

struct GanakStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
    /// Child-process CPU time (user+sys), from wait4(); unlike total_time_s
    /// (wall) it excludes time the parent spends blocked waiting on the child.
    inline static double total_cpu_s = 0.0;
    /// Counts abandoned at the per-call timeout, raised as errors so the
    /// individual is dropped (counted against max_scoring_failure_rate).
    /// Counts execs, not calls: run_ganak_on_formula memoises the
    /// abandonment, and a repeat of the same formula and seed re-raises it as
    /// a cache hit without paying the budget again.
    inline static std::size_t n_timeouts = 0;
};

/// Raised when the ganak exec is abandoned at its per-call budget, as opposed
/// to ganak itself failing. The two are distinct types because only the first
/// is a property of the formula: a non-zero exit can be a transient spawn
/// failure under heavy concurrent forking, which must not be memoised as a
/// permanent one.
class GanakTimeout : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/// Returns the path to the ganak binary: `COUNTER_GANAK_PATH` when that
/// environment variable is set and non-empty, and otherwise the
/// GANAK_EXECUTABLE_PATH preprocessor definition baked in at build time. The
/// environment is read once, on first use.
std::string ganak_executable_path();

/// When cpu_s_out is non-null it receives the child's user+sys CPU time in
/// seconds (from wait4), letting run_ganak_on_formula attribute CPU to ganak.
Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed = 1,
                          double* cpu_s_out = nullptr);

/// Counts the models of `formula` over the variables it mentions, memoised on
/// the canonical renamed key (`formula_key::renamed`) and the seed.
///
/// The renaming is sound here where it is not in `simplify_ltl`, because the
/// value is a number rather than a formula: a count is invariant under a
/// bijection on the atoms, and the free variables of the wider alphabet are
/// multiplied back in by the caller, outside this cache. Two guards differing
/// only in operand order, association or atom naming therefore share one
/// exec, which over nine specifications is 20.3% to 49.5% of the execs a run
/// makes.
Count run_ganak_on_formula(const std::string& formula, unsigned seed = 1);
