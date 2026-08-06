#pragma once

/// @file ganak.hpp
/// @brief Wrapper around the Ganak weighted model counter, used to count
///        satisfying valuations for automaton transitions.

#include <chrono>
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

std::string ganak_executable_path();

/// Per-call wall-clock budget for the ganak exec (process-global, like the
/// other tool budgets). A call exceeding it is killed and raised as an error,
/// dropping the individual rather than stalling the run. Zero (the default)
/// disables the timeout: counting is the fitness function's real work, so a
/// slow count is usually a legitimately hard one, and abandoning it spends the
/// run's scoring-failure tolerance. Set once at startup from
/// Config::ganak_timeout.
void set_ganak_timeout(std::chrono::milliseconds timeout);

/// When cpu_s_out is non-null it receives the child's user+sys CPU time in
/// seconds (from wait4), letting run_ganak_on_formula attribute CPU to ganak.
Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed = 1,
                          double* cpu_s_out = nullptr);

Count run_ganak_on_formula(const std::string& formula, unsigned seed = 1);
