#pragma once

#include <string>
#include <unordered_map>

#include "requirement.hpp"

std::string spot_bin_dir();
std::string ltlsynt_path();
std::string ltl2tgba_path();

/// Runs ltl2tgba on the given LTL formula and returns the HOA output.
/// Uses -D (deterministic) -S (state-based acceptance) -H (HOA format).
std::string run_ltl2tgba_for_counting(const std::string& formula);

class RealizabilityChecker {
   public:
    bool check_realizability(const Specification& specification);

   private:
    std::unordered_map<std::string, bool> m_cache;
};
