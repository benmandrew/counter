#pragma once

/// @file config.hpp
/// @brief Compile-time algorithm parameters for the genetic repair loop.

#include <chrono>
#include <cstddef>
#include <thread>

/// Compile-time algorithm parameters for the genetic repair loop.
/// All values are static constexpr except black_timeout and n_hw_threads,
/// which must be runtime values (overridable in tests and queried at startup).
struct Config {
    /// Number of generations the genetic loop runs before collecting survivors.
    static constexpr std::size_t generations = 10;

    /// Number of specifications maintained per generation. Larger values
    /// improve coverage at the cost of more fitness evaluations per round.
    static constexpr std::size_t population_size = 200;

    /// Weight of the syntactic similarity component in the aggregated fitness
    /// score. Higher values bias repair towards structurally similar
    /// candidates.
    static constexpr double fitness_weight_syntactic = 0.2;

    /// Weight of the semantic similarity component (bounded model counting).
    /// Dominant weight: captures trace-level behavioural proximity.
    static constexpr double fitness_weight_semantic = 0.5;

    /// Weight of the Halstead complexity penalty. Keeps repairs from growing
    /// unnecessarily large relative to the original specification.
    static constexpr double fitness_weight_halstead = 0.1;

    /// Weight of the realizability status score (0–1 from
    /// specification_status). Equal to fitness_weight_semantic so realizable
    /// candidates are strongly preferred.
    static constexpr double fitness_weight_status = 0.5;

    /// Contribution of trigger formula similarity to the syntactic score.
    static constexpr double syntactic_weight_trigger = 1.0;

    /// Contribution of response formula similarity to the syntactic score.
    static constexpr double syntactic_weight_response = 1.0;

    /// Contribution of timing constraint similarity to the syntactic score.
    static constexpr double syntactic_weight_timing = 1.0;

    /// Trace-length bound k for model counting. Larger values count more traces
    /// but increase Ganak runtime. Must be ≥ the longest timing tick count in
    /// the specification.
    static constexpr std::size_t default_model_counting_bound = 20;

    /// Enable the per-generation weakening filter, which discards candidates
    /// not logically implied by the original specification.
    static constexpr bool run_weakening_filter = true;

    /// Enable the final implication filter, which keeps only specifications
    /// that are maximal under the implication partial order.
    static constexpr bool run_implication_filter = true;

    /// Timeout for each black satisfiability query. Not constexpr: tests
    /// override this to a larger value at startup because a timeout is a sound
    /// "not proven" result during a run but would falsely fail test assertions
    /// that expect a definite SAT/UNSAT answer on slow CI machines.
    inline static std::chrono::milliseconds black_timeout{1000};

    /// Probability that a parent pair undergoes crossover instead of one parent
    /// being copied directly. In [0, 1].
    static constexpr double crossover_rate = 0.1;

    /// Probability that the selected parent (post-crossover) is mutated.
    /// Set to 1.0 so every offspring receives at least one mutation.
    static constexpr double mutation_rate = 1.0;

    /// Per-requirement probability of mutating the condition (trigger) formula.
    static constexpr double p_trigger = 0.5;

    /// Per-requirement probability of mutating the response formula.
    static constexpr double p_response = 0.5;

    /// Per-requirement probability of mutating the timing constraint.
    /// Lower than p_trigger/p_response because timing changes are more
    /// disruptive to semantic similarity.
    static constexpr double p_timing = 0.15;

    /// Number of hardware threads available; used to size the global thread
    /// pool for concurrent fitness evaluation.
    inline static const std::size_t n_hw_threads =
        std::thread::hardware_concurrency();
};
