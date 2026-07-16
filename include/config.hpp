#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

/// Selection scheme driving parent and survivor selection during evolution.
/// Nsga2 (the default) ranks candidates by Pareto non-domination and crowding
/// distance over the individual objectives, searching for the Pareto front
/// rather than one weighted compromise. WeightedAverage ranks them by the
/// single blended fitness scalar; it converges prematurely and is retained for
/// comparison rather than use.
enum class SelectionScheme : std::uint8_t { WeightedAverage, Nsga2 };

/// Metric turning the bounded trace counts into a semantic-similarity score.
/// Direct (the default) takes the ratio of counts -- the fraction of one
/// requirement's satisfying traces that also satisfy the other, a Sorensen-Dice
/// overlap. Logarithmic takes the ratio of the counts' logarithms, comparing
/// the languages' growth rates instead: unlike the direct ratio it stays
/// roughly constant as the counting bound grows, rather than decaying toward
/// zero for requirements of differing permissiveness.
enum class SimilarityMetric : std::uint8_t { Direct, Logarithmic };

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
    SimilarityMetric similarity_metric = SimilarityMetric::Direct;
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
    SelectionScheme selection_scheme = SelectionScheme::Nsga2;
    double selection_rate = 0.5;
    // Elitism: the top elitism_rate fraction of the population carries over
    // into the next generation verbatim, bypassing crossover, mutation, and
    // the offspring filters, so the best candidates are never lost to a
    // stochastic operator. Must be strictly less than selection_rate (the
    // elites are a subset of the selected parents).
    double elitism_rate = 0.1;
    double crossover_rate = 0.1;
    double mutation_rate = 1.0;
    double p_trigger = 0.5;
    double p_response = 0.5;
    double p_timing = 0.15;
    // Low-probability structural mutation, shared by both modes: append a new
    // environment assumption (over input atoms) rather than rewriting an
    // existing requirement/formula. This is how the algorithm can repair
    // unrealizability that requires strengthening the environment (e.g. adding
    // a fairness assumption to an unrealizable GR(1) spec), which the
    // rewrite-only operators cannot express.
    double p_add_assumption = 0.05;
    // TLSF-mode mutation: probability of selecting an assumption-side section
    // (INITIALLY/REQUIRE/ASSUME) versus a guarantee-side section
    // (PRESET/ASSERT/GUARANTEE) when mutating a tlsf::Specification.
    double tlsf_p_assumption = 0.3;
    double tlsf_p_guarantee = 0.7;
    // TLSF-mode mutation: once a section formula has been chosen for rewriting,
    // the probability of applying the temporal-structure mutation (which may
    // insert, drop, or swap X/F/G/U/R/W operators, following Brizzio et al.)
    // rather than the skeleton-preserving propositional rewrite. At 0 the
    // temporal skeleton of existing formulae is never altered.
    double tlsf_p_temporal = 0.2;
    std::size_t parallel = std::thread::hardware_concurrency();
    // A fitness function that throws (in practice an external tool failing on
    // one evolved formula) costs that individual rather than the whole run:
    // the search is stochastic, so one candidate lost out of a population is
    // noise, while aborting at generation 23 of 40 loses everything. Above
    // this fraction of a generation the tooling is broken rather than the
    // formula, and the run aborts instead of evolving noise into the output.
    // A single failure is always tolerated, whatever the population size.
    double max_scoring_failure_rate = 0.05;
};
