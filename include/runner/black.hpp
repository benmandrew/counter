#pragma once

#include <string>
#include <unordered_map>

std::string black_executable_path();

class SatisfiabilityChecker {
   public:
    bool check_satisfiability(const std::string& ltl_formula);

   private:
    std::unordered_map<std::string, bool> m_cache;
};
