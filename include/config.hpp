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
    // Per-generation filters run only every Nth generation (1 = every
    // generation). The final generation always runs every filter, so the
    // resulting population is never left un-deduplicated/un-weakened.
    std::size_t dedup_filter_interval = 1;
    std::size_t false_condition_filter_interval = 1;
    std::size_t weakening_filter_interval = 1;
    std::size_t bloat_filter_interval = 1;
    std::chrono::milliseconds black_timeout{1000};
    // When true, print the CPU-attribution report (your code vs. the external
    // CLI tools, via getrusage + per-tool wait4). Opt-in: off leaves output
    // identical to before.
    bool report_cpu_timing = false;
    double selection_rate = 0.5;
    double crossover_rate = 0.1;
    double mutation_rate = 1.0;
    double p_trigger = 0.5;
    double p_response = 0.5;
    double p_timing = 0.15;
    // TLSF-mode mutation: probability of selecting an assumption-side section
    // (INITIALLY/REQUIRE/ASSUME) versus a guarantee-side section
    // (PRESET/ASSERT/GUARANTEE) when mutating a tlsf::Specification.
    double tlsf_p_assumption = 0.3;
    double tlsf_p_guarantee = 0.7;
    std::size_t parallel = std::thread::hardware_concurrency();
};
