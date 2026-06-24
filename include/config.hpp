#pragma once

#include <chrono>
#include <cstddef>
#include <thread>

struct Config {
    std::size_t generations = 10;
    std::size_t population_size = 200;
    double fitness_weight_syntactic = 0.2;
    double fitness_weight_semantic = 0.5;
    double fitness_weight_halstead = 0.1;
    double fitness_weight_status = 0.5;
    double syntactic_weight_trigger = 1.0;
    double syntactic_weight_response = 1.0;
    double syntactic_weight_timing = 1.0;
    std::size_t default_model_counting_bound = 20;
    bool run_weakening_filter = true;
    bool run_implication_filter = true;
    std::chrono::milliseconds black_timeout{1000};
    double crossover_rate = 0.1;
    double mutation_rate = 1.0;
    double p_trigger = 0.5;
    double p_response = 0.5;
    double p_timing = 0.15;
    std::size_t parallel = std::thread::hardware_concurrency();
};
