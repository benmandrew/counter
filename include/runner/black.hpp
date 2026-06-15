#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

std::string black_executable_path();

class SatisfiabilityChecker {
   public:
    inline static std::size_t n_cache_misses = 0;
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_timeouts = 0;
    inline static double total_time_s = 0.0;

    /// Returns true (SAT), false (UNSAT), or nullopt (timed out / unknown).
    std::optional<bool> check_satisfiability(const std::string& ltl_formula);

   private:
    mutable std::mutex m_cache_mutex;
    std::unordered_map<std::string, bool> m_cache;
};

/// Returns the process-lifetime SatisfiabilityChecker instance. All callers
/// that do not need test isolation should use this instead of constructing
/// their own, so they share the memoisation cache.
SatisfiabilityChecker& global_sat_checker();
