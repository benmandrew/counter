#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

std::string black_executable_path();

class SatisfiabilityChecker {
   public:
    inline static std::size_t n_cache_misses = 0;
    inline static std::size_t n_cache_hits = 0;
    inline static double total_time_s = 0.0;

    bool check_satisfiability(const std::string& ltl_formula);

   private:
    mutable std::mutex m_cache_mutex;
    std::unordered_map<std::string, bool> m_cache;
};
