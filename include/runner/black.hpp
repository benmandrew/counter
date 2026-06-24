#pragma once

/// @file black.hpp
/// @brief Wrapper around the black LTL satisfiability checker, with a
///        memoising SatisfiabilityChecker and a process-lifetime global
///        instance.

#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

std::string black_executable_path();

class SatisfiabilityChecker {
   public:
    inline static std::atomic<std::size_t> n_cache_misses{0};
    inline static std::atomic<std::size_t> n_cache_hits{0};
    inline static std::size_t n_timeouts = 0;
    inline static double total_time_s = 0.0;

    /// Returns true (SAT), false (UNSAT), or nullopt (timed out / unknown).
    std::optional<bool> check_satisfiability(const std::string& ltl_formula);

    void set_timeout(std::chrono::milliseconds timeout) { m_timeout = timeout; }

   private:
    std::chrono::milliseconds m_timeout{1000};
    // Cache lookups (the common case once the population converges) take a
    // shared lock so concurrent hits don't serialise on one another; only an
    // actual insert needs the exclusive lock.
    mutable std::shared_mutex m_cache_mutex;
    std::unordered_map<std::string, std::optional<bool>> m_cache;
};

/// Returns the process-lifetime SatisfiabilityChecker instance. All callers
/// that do not need test isolation should use this instead of constructing
/// their own, so they share the memoisation cache.
SatisfiabilityChecker& global_sat_checker();
