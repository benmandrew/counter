#pragma once

#include <chrono>
#include <cstddef>
#include <thread>

struct Config {
    // --- Run parameters ---
    static constexpr std::size_t generations = 10;
    static constexpr std::size_t population_size = 100;

    // --- Fitness weights ---
    static constexpr double fitness_weight_syntactic = 0.25;
    static constexpr double fitness_weight_semantic = 0.25;
    static constexpr double fitness_weight_halstead = 0.1;
    static constexpr double fitness_weight_status = 0.5;

    // --- Model counting ---
    static constexpr std::size_t default_model_counting_bound = 20;

    // --- Filtering ---
    static constexpr bool run_implication_filter = true;

    // --- External tool timeouts ---
    static constexpr std::chrono::seconds black_timeout{10};

    // --- Evolution ---
    static constexpr double crossover_rate = 0.1;
    static constexpr double mutation_rate = 1.0;

    // --- Requirement mutation probabilities ---
    static constexpr double p_trigger = 0.5;
    static constexpr double p_response = 0.5;
    static constexpr double p_timing = 0.1;

    // --- Hardware ---
    inline static const std::size_t n_hw_threads =
        std::thread::hardware_concurrency();
};
