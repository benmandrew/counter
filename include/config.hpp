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
/// Direct takes the ratio of counts -- the fraction of one requirement's
/// satisfying traces that also satisfy the other, a Sorensen-Dice overlap.
/// Logarithmic (the default) takes the ratio of the counts' logarithms,
/// comparing the languages' growth rates instead: unlike the direct ratio it
/// stays roughly constant as the counting bound grows, rather than decaying
/// toward zero for requirements of differing permissiveness. It is the default
/// because it recovered more ideal repairs across every spec in the direct-vs-
/// log campaign (overall implies-ideal 68.8% vs 61.5%, decisively on fsm).
enum class SimilarityMetric : std::uint8_t { Direct, Logarithmic };

/// How TLSF repair searches. Monolithic (the default) evolves the whole
/// specification at once. Muc repairs iteratively: it extracts a minimal
/// unrealizable core, evolves only that sub-specification, reintegrates the
/// repaired core with the untouched non-core guarantees, and repeats on the
/// recombined spec until it is realizable (or the iteration cap trips). The
/// mode is TLSF-only; the FRETISH path ignores it.
enum class RepairMode : std::uint8_t { Monolithic, Muc };

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
    SimilarityMetric similarity_metric = SimilarityMetric::Logarithmic;
    bool run_weakening_filter = true;
    bool run_implication_filter = true;
    // Drop candidates whose assumptions are jointly unsatisfiable: they are
    // realizable for free, since a false antecedent makes
    // (assumptions) -> (guarantees) a tautology. A no-op for specifications
    // with no assumptions, which short-circuit before any solver call.
    bool run_vacuity_filter = true;
    // Drop candidates that are not well-separated: ones the system can satisfy
    // vacuously by forcing its own assumptions to fail. Realizability is
    // decided on (assumptions) -> (guarantees), so replacing the guarantees
    // with false and finding (assumptions) -> false realizable means the system
    // has a strategy that breaks the assumptions on its own. Strictly stronger
    // than the vacuity check, but each test is a full ltlsynt query, so off by
    // default; the interval throttles it when enabled. A no-op for specs with
    // no assumptions, which short-circuit before any solver call.
    bool run_well_separation_filter = false;
    // Per-generation filters run only every Nth generation (1 = every
    // generation). The final generation always runs every filter, so the
    // resulting population is never left un-deduplicated/un-weakened.
    std::size_t dedup_filter_interval = 1;
    std::size_t false_condition_filter_interval = 1;
    std::size_t weakening_filter_interval = 1;
    std::size_t bloat_filter_interval = 1;
    std::size_t vacuity_filter_interval = 1;
    std::size_t well_separation_filter_interval = 1;
    std::chrono::milliseconds black_timeout{1000};
    // Per-call wall-clock budget for ltlsynt realizability checks. Unlike
    // black, ltlsynt has no internal timeout, and the genetic search
    // occasionally generates synthesis queries that run for minutes with no
    // upper bound, stalling a run on the tail. A call exceeding this is killed
    // and treated as unrealizable. 0 (the default) disables the timeout,
    // preserving prior behaviour; the heavy TLSF specs set it.
    std::chrono::milliseconds ltlsynt_timeout{0};
    // Per-call wall-clock budget for the ltl2tgba model-counting exec. Like
    // ltlsynt, ltl2tgba has no internal timeout, and the deterministic (-D)
    // construction blows up super-exponentially on the deeply nested formulae
    // the search occasionally builds (multi-GB, minutes-to-hours). A call
    // exceeding this is killed and the individual is dropped (counted against
    // max_scoring_failure_rate). 0 (the default) disables the timeout,
    // preserving prior behaviour; the heavy TLSF specs set it.
    std::chrono::milliseconds ltl2tgba_timeout{0};
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
    // Of the assumptions p_add_assumption appends, the fraction guarded by a
    // random input atom rather than by `true`. G F <input> is strictly stronger
    // than G(c -> F <input>) and so the more powerful repair, which is why the
    // unconditional form keeps the majority of the draw.
    double p_conditional_assumption = 0.25;
    // Mutate assumption timings in the strengthening direction rather than the
    // weakening one. Weakening the overall assume-guarantee specification means
    // weakening a guarantee but strengthening an assumption, so weakening both
    // makes every assumption mutation a move away from a repair. Retained as a
    // flag only so the two directions can be crossed as an experiment factor.
    bool strengthen_assumptions = true;
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
    // TLSF repair strategy (see RepairMode). Muc mode caps its outer
    // extract-repair-reintegrate loop at muc_max_iterations, so a spec whose
    // core never becomes realizable ends the run without a repair rather than
    // looping forever.
    RepairMode repair_mode = RepairMode::Monolithic;
    std::size_t muc_max_iterations = 32;
    std::size_t parallel = std::thread::hardware_concurrency();
    // Upper bound on ltlsynt processes running concurrently across the whole
    // program, independent of `parallel`. ltlsynt is by far the heaviest
    // external tool on hard specs (multi-GB resident per call for the TLSF
    // examples), so a scoring pool of `parallel` workers each spawning one can
    // exhaust RAM and OOM the machine. 0 means unlimited (the default, which
    // preserves prior behaviour); a positive value serialises the surplus while
    // the other workers keep doing non-ltlsynt work. Size it to fit RAM:
    // roughly (available_GB / per-call_GB).
    std::size_t max_concurrent_realizability = 0;
    // A fitness function that throws (in practice an external tool failing on
    // one evolved formula) costs that individual rather than the whole run:
    // the search is stochastic, so one candidate lost out of a population is
    // noise, while aborting at generation 23 of 40 loses everything. Above
    // this fraction of a generation the tooling is broken rather than the
    // formula, and the run aborts instead of evolving noise into the output.
    // A single failure is always tolerated, whatever the population size.
    double max_scoring_failure_rate = 0.05;
};
