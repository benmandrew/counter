#pragma once

#include <string>

#include "fitness/transfer_matrix.hpp"

struct GanakStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
};

std::string ganak_executable_path();

Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed = 1);

Count run_ganak_on_formula(const std::string& formula, unsigned seed = 1);
