#pragma once

#include <cstddef>

struct Config {
    // --- Run parameters ---
    static constexpr std::size_t generations = 20;
    static constexpr std::size_t population_size = 100;

    // --- Fitness weights ---
    static constexpr double fitness_weight_syntactic = 0.25;
    static constexpr double fitness_weight_semantic = 0.25;
    static constexpr double fitness_weight_status = 0.5;

    // --- Evolution ---
    static constexpr double crossover_rate = 0.1;
    static constexpr double mutation_rate = 1.0;

    // --- Requirement mutation probabilities ---
    static constexpr double p_trigger = 1.0;
    static constexpr double p_response = 1.0;
    static constexpr double p_timing = 0.1;
};
