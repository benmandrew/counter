#pragma once

#include <string>
#include <unordered_map>

#include "requirement.hpp"

std::string spot_bin_dir();
std::string ltlsynt_path();

class RealizabilityChecker {
   public:
    bool check_realizability(const Requirement& requirement);

   private:
    std::unordered_map<std::string, bool> m_cache;
};
