#pragma once

/// @file ganak.hpp
/// @brief Wrapper around the Ganak weighted model counter, used to count
///        satisfying valuations for automaton transitions.

#include <string>

#include "fitness/transfer_matrix.hpp"

struct GanakStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
    // Child-process CPU time (user+sys), from wait4(); unlike total_time_s
    // (wall) it excludes time the parent spends blocked waiting on the child.
    inline static double total_cpu_s = 0.0;
};

std::string ganak_executable_path();

// When cpu_s_out is non-null it receives the child's user+sys CPU time in
// seconds (from wait4), letting run_ganak_on_formula attribute CPU to ganak.
Count run_ganak_on_dimacs(const std::string& dimacs_path, unsigned seed = 1,
                          double* cpu_s_out = nullptr);

Count run_ganak_on_formula(const std::string& formula, unsigned seed = 1);
