#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

/// Selection scheme driving parent and survivor selection during evolution.
/// The two NSGA-II schemes rank identically -- by Pareto non-domination and
/// crowding distance over the individual objectives, searching for the Pareto
/// front rather than one weighted compromise -- and are named for the only step
/// where they differ, the survivor step. Nsga2Truncate ranks the pooled
/// (mu + lambda) parents and offspring and cuts the pool at the target size.
/// Nsga2Apportion (the default) deduplicates the pool first, ranks the distinct
/// set, and apportions the target size's slots over it by 1 / (1 + rank) under
/// the largest-remainder (Hamilton) method. A truncated pool holds only a
/// handful of distinct specifications across its slots, so the cut runs
/// arbitrarily through the Pareto front; apportioning instead makes breeding
/// pressure proportional to rank without discarding any distinct candidate.
/// WeightedAverage ranks by the single blended fitness scalar; it converges
/// prematurely and is retained for comparison rather than use.
enum class SelectionScheme : std::uint8_t {
    WeightedAverage,
    Nsga2Truncate,
    Nsga2Apportion
};

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

/// How the status objective grades the region below realizability. Tiered is
/// the three-point scale in fitness/status.hpp. Mrs (the default) replaces its
/// middle tier with the greedy maximal-realizable-subset fraction: the
/// guarantee side is split into parts, and the score is what fraction of them
/// can be kept while the accumulated subset stays realizable against the full,
/// unchanged environment side.
///
/// The two stay crossed as an experiment factor, so a campaign can still ask
/// for Tiered by name. Grading is the whole point: over the 21 specifications
/// in `examples/`, Tiered scores
/// every one of them 0.5, where Mrs spreads them over 14 distinct values with a
/// median of 6 grade levels per specification.
enum class StatusGrading : std::uint8_t { Tiered, Mrs };

/// Which order the MRS walk admits guarantee-side parts in (see
/// @ref status_score_mrs). Spec is the index order the walk shipped with;
/// Degree is @ref conflict_degree_order, computed once on the input
/// specification and replayed on every candidate.
///
/// Both are constant across a run, which is what the walk needs: the cache and
/// the seed reproducibility both turn on every candidate walking the *same*
/// order, not on which one. Only Degree pays to choose it, at n(n-1)/2 + n
/// subset queries once.
enum class MrsAdmissionOrder : std::uint8_t { Spec, Degree };

struct Config {
    std::size_t generations = 10;
    std::size_t population_size = 200;
    double fitness_weight_syntactic = 0.2;
    double fitness_weight_semantic = 0.5;
    double fitness_weight_status = 0.5;
    /// How the status objective grades below realizability (see StatusGrading).
    /// Mrs costs more realizability queries per candidate -- a median of 4.6x
    /// one whole-specification check across `examples/`, falling to 2.2x over a
    /// population, since the greedy prefixes recur across near-identical
    /// candidates and RealizabilityChecker memoises by formula string.
    ///
    /// Mrs is the default on the 2026-08-11 status-grading campaign, which
    /// paired the two over 20 TLSF specifications x 24 seeds: yield 410/480
    /// against 367/480 (50 Mrs-only pairs to 7, sign test \f$p < 10^{-4}\f$),
    /// repair quality unchanged where both arms yield, at a median paired wall
    /// cost of 1.15x. The gain concentrates where Tiered cannot grade at all --
    /// `arbiter` moves 0/24 to 22/24 and `rg1` 7/24 to 24/24.
    StatusGrading status_grading = StatusGrading::Mrs;
    /// Which order the MRS walk admits parts in (see MrsAdmissionOrder). Read
    /// only under StatusGrading::Mrs.
    ///
    /// Degree is the default on a population measurement rather than on a
    /// campaign, which is weaker evidence than status_grading above rests on.
    /// Over populations of `tlsf_mutate` mutants across six TLSF
    /// specifications, scored in isolation, Degree scores 0.587 against Spec's
    /// 0.529 at 1.02x the ltlsynt execs, and scored no lower than Spec on any
    /// of the six. Those populations carry no selection pressure, so what the
    /// finer gradient does to yield or implies_ideal is still unmeasured, and a
    /// paired campaign is still owed.
    ///
    /// Moving this default changes what every archived config *means*, since
    /// none of them state a key that did not exist when they ran. See "Config
    /// vintage" in experiments/README.md, and VINTAGE_KEYS in
    /// scripts/gen_configs.py.
    MrsAdmissionOrder mrs_admission_order = MrsAdmissionOrder::Degree;
    std::size_t default_model_counting_bound = 20;
    SimilarityMetric similarity_metric = SimilarityMetric::Logarithmic;
    /// Keep only repairs the original logically implies -- genuine weakenings.
    /// A *final* screen, not a per-generation filter: pruning non-weakenings
    /// mid-search measurably costs repair quality and never gains it (over the
    /// 9,796 paired runs of the cj-large campaign it lost 1,005 and won 410,
    /// costing 20 points of implies-ideal on fsm), whereas screening the final
    /// population leaves the search bit-identical.
    ///
    /// **Off by default since 2026-08-20**, on the measurement in
    /// `experiments/2026-08-20-ops-weakening/REPORT.md`. Across 720 matched
    /// runs of two paired campaigns, turning the screen off gained
    /// `implies_ideal` on 38 of them and lost it on none, on both paths and
    /// under both mutation grammars. That direction is close to structural
    /// rather than discovered -- the screen draws nothing from the
    /// `RandomSource`, so it cannot change the search and can only withhold
    /// output -- so the counts are the finding and a significance test on them
    /// would overstate it. It is not a strict superset either: `n_repairs`
    /// fell on 27 of the 720, the implication filter below running afterwards
    /// and letting a newly admitted non-weakening dominate several weakenings.
    /// The earlier 2026-08-19-weakening-arbiter campaign found the same
    /// direction at 9 of 120 paired repairs.
    ///
    /// What it costs when off is the only guarantee that a written repair does
    /// not forbid behaviour the original allowed. Nothing constrains mutation
    /// to weaken: it can delete a safety guarantee and add a stronger one,
    /// reaching realizability while forbidding allowed behaviour. Turn it on
    /// where that property is wanted over yield.
    ///
    /// Applies on both paths, but the check behind it differs in strength. The
    /// FRETISH spec_implies is an assume-guarantee decomposition that pairs
    /// each requirement against a single counterpart, and so under-detects: it
    /// can reject a genuine weakening that only holds via several requirements
    /// together. The TLSF tlsf_spec_implies lowers the whole specification to
    /// one LTL formula and is exact, so a rejection there is a fact about the
    /// two specs. Expect the TLSF screen to reject more, and to be right when
    /// it does.
    bool run_weakening_filter = false;
    bool run_implication_filter = true;
    /// Drop candidates that hold for free rather than because anything was
    /// repaired: ones carrying a requirement whose condition is the literal
    /// `false`, ones with a *valid* guarantee, which demands nothing, and ones
    /// whose assumptions are jointly
    /// unsatisfiable, since a false antecedent makes
    /// (assumptions) -> (guarantees) a tautology. Applied in that order, which
    /// is cheapest first: the syntactic screen costs no solver call, the
    /// guarantee queries are small and cached per requirement, and the
    /// assumption query is one large one, skipped entirely when a specification
    /// has no assumptions.
    ///
    /// Search pressure only. Collecting the repairs applies the same predicate
    /// unconditionally on both paths, so turning this off never admits a
    /// vacuous repair to the output.
    bool run_vacuity_filter = true;
    /// Drop candidates that are not well-separated: ones the system can satisfy
    /// vacuously by forcing its own assumptions to fail. Realizability is
    /// decided on (assumptions) -> (guarantees), so replacing the guarantees
    /// with false and finding (assumptions) -> false realizable means the
    /// system has a strategy that breaks the assumptions on its own.
    /// Complementary to the vacuity check. Each test is a full ltlsynt query
    /// (run only when an assumption references an output atom), which is why
    /// this was off by default until the 2026-08-06 wellsep-timing campaign
    /// priced it: over 7200 TLSF runs, filtering every generation came out 5%
    /// *faster* than not filtering at all, because a candidate dropped before
    /// the scoring stage never costs a model-count or a synthesis query. Unlike
    /// run_weakening_filter this stays a per-generation filter rather than
    /// becoming a final screen -- the same campaign measured an end-of-run pass
    /// leaking 42.7% against 44.1% for no filter at all, since elites and
    /// NSGA-II parent pooling re-admit whatever a late pass drops. It is the
    /// counterpart to allow_output_assumptions, which without it admits
    /// assumptions the system can defeat. A no-op for specs with no
    /// assumptions, which short-circuit before any solver call.
    ///
    /// Off by default since well-separation joined the status score. Filters
    /// run before scoring, so leaving this on drops an ill-separated candidate
    /// before anything can score it, and the status tier that ranks it below a
    /// genuine repair never fires -- the same way an earlier assumption-side
    /// tier sat unreachable behind the vacuity filter. On costs less wall time
    /// and gives the search no gradient off ill-separation; off pays to score
    /// candidates the search then ranks down. Output correctness does not turn
    /// on it either way: the gate screens every survivor whatever this says.
    bool run_well_separation_filter = false;
    std::chrono::milliseconds black_timeout{1000};
    /// Per-call wall-clock budget for ltlsynt realizability checks. Unlike
    /// black, ltlsynt has no internal timeout, and the genetic search
    /// occasionally generates synthesis queries that run for minutes with no
    /// upper bound, stalling a run on the tail. A call exceeding this is killed
    /// and reported as undecided, which each caller resolves its own way: no
    /// repair is admitted on it, and the well-separation filter drops the
    /// candidate. 0 disables the timeout.
    ///
    /// 10 s by default. It was 500 ms, on the measurement that the call
    /// durations are sharply bimodal: 95% of an archived TLSF campaign's calls
    /// finished under 50 ms, the 0.5-1 s band was almost empty, and a 500 ms
    /// cap abandoned within 0.1% of the calls a 10 s cap did. That statistic is
    /// true and was the wrong one to tune on, because it counts calls across
    /// the corpus rather than per specification. A specification whose *own*
    /// realizability query exceeds the budget does not lose 0.1% of its calls,
    /// it loses all of them: every candidate comes back undecided, the status
    /// objective becomes a constant costing a full budget per distinct
    /// candidate, and the final gate rejects even a correct repair. `amba` is
    /// exactly that case -- 1.5 s for the unrealizable original and 6.1 s to
    /// prove its known-good repair realizable, against a 500 ms budget.
    ///
    /// 10 s clears the corpus worst case by well over a factor of one and is
    /// the value the four archived configs that overrode 500 ms chose. It
    /// still cuts the minutes-long tail the timeout exists for. Losing one
    /// candidate out of a population to that tail is the same noise
    /// max_scoring_failure_rate already tolerates; an unbounded call costs the
    /// whole run. Raising this does not rescue a run already under way: an
    /// undecided verdict is memoised like any other (see RealizabilityChecker),
    /// so a formula abandoned once stays undecided for the rest of the run.
    ///
    /// scripts/gen_configs.py carries its own TLSF_LTLSYNT_TIMEOUT_MS, which a
    /// generated TLSF campaign emits explicitly and which therefore overrides
    /// this default rather than following it.
    std::chrono::milliseconds ltlsynt_timeout{10'000};
    /// Per-call wall-clock budget for the ltl2tgba model-counting exec. Like
    /// ltlsynt, ltl2tgba has no internal timeout, and the deterministic (-D)
    /// construction blows up super-exponentially on the deeply nested formulae
    /// the search occasionally builds (multi-GB, minutes-to-hours). A call
    /// exceeding this is killed and the individual is dropped (counted against
    /// max_scoring_failure_rate). 0 disables the timeout.
    ///
    /// 60 s by default, the value every archived TLSF campaign set. Far looser
    /// than the ltlsynt budget because a legitimate determinisation can take
    /// tens of seconds, where an ltlsynt call that has not finished in half a
    /// second is in the tail. This is the budget that bounds the multi-GB
    /// blowups: an untimed construction has been seen to run for hours and
    /// leak orphaned processes across a long run.
    std::chrono::milliseconds ltl2tgba_timeout{60'000};
    /// Per-call wall-clock budget for each ltlfilt exec. Unlike the budgets
    /// above this defaults to a real value rather than to "off": `--simplify`
    /// is
    /// super-exponential on the deep nested-X conjunctions the search builds,
    /// and an abandoned call costs only a missed simplification, never an
    /// individual.
    std::chrono::milliseconds ltlfilt_timeout{10'000};
    /// Per-call wall-clock budget for each ganak model-counting exec. 0 (the
    /// default) disables it: counting is the fitness function's real work, so a
    /// slow count is usually a legitimately hard one rather than a blowup, and
    /// abandoning it drops the individual against max_scoring_failure_rate.
    std::chrono::milliseconds ganak_timeout{0};
    /// When true, print the CPU-attribution report (your code vs. the external
    /// CLI tools, via getrusage + per-tool wait4). Set by `counter
    /// --cpu-report` alone; deliberately not a TOML key, because it asks a
    /// question about one interactive run rather than about the search, exactly
    /// as COUNTER_PROFILE does for the scope profiler.
    bool report_cpu_timing = false;
    /// When true, print the engine-internal counters at exit: per-tool call and
    /// cache totals, the ltl2tgba tautology substitutions, the constant-folded
    /// count, and the fitness cache hit rate. Set by `counter --diagnostics`
    /// alone, a flag rather than a TOML key for the same reason
    /// report_cpu_timing is. Off by default because stdout is for watching a
    /// run in progress and none of these say anything about the run's repairs.
    /// Nothing is lost by omitting them: every one is written to
    /// `<output-dir>/run.json` on every successful run either way.
    bool report_diagnostics = false;
    /// When true, stream per-stage and per-generation progress to
    /// `<output-dir>/progress.jsonl` and copy the dashboard page beside it.
    /// Opt-in
    /// because a campaign of many runs pays for the extra file and its flushes
    /// without anyone watching. The `--dashboard` flag turns it on for one run
    /// without editing the config.
    bool dashboard = false;
    /// Survivor step for the two NSGA-II schemes, which rank identically.
    /// Apportion since 2026-08-14, on the `2026-08-11-selection-default`
    /// campaign: it finds a repair more often on both paths (FRETISH 0.654 to
    /// 0.704, TLSF 0.857 to 0.887) and returns more of them (+1.82 per TLSF
    /// run, up on 19 of 20 specifications), for 1.35x the FRETISH wall time and
    /// 1.15x the TLSF.
    ///
    /// That campaign's pre-registered rule said *no*, and this default was
    /// changed against it. Three of its five criteria failed: FRETISH quality
    /// against the compute-matched arm (-0.0250, interval reaching -0.0536),
    /// and the yield gain against both comparators, whose intervals include 1
    /// (TLSF against compute-matched: odds ratio 1.786, p = 0.078). They failed
    /// on interval width rather than direction -- every yield point estimate
    /// favours apportion, and per unit of extra compute it beats spending the
    /// same budget on more generations. The measured loss is 5 FRETISH runs in
    /// 280 that stop matching an ideal, against 14 more that find a repair at
    /// all. docs/configuration.rst records both sides and the untested ground.
    SelectionScheme selection_scheme = SelectionScheme::Nsga2Apportion;
    /// Report every candidate that passed the output gate in *any* generation,
    /// not only those the final population still holds. A candidate that was
    /// gate-passing in generation 3 and was not selected into generation 4 is
    /// otherwise discarded, even though repair quality is judged existentially
    /// over what the run emits, so a larger pool can only help it.
    ///
    /// Off by default because it changes what a run emits, and an archived
    /// campaign config that omits the key would otherwise silently mean
    /// something it did not (see the config vintage note in
    /// experiments/README.md). It costs nothing to switch on for the FRETISH
    /// path, whose per-generation "real" counter already asks the gate; on the
    /// TLSF path, which asks it once after evolution, it buys the extra repairs
    /// with one gate sweep per generation.
    ///
    /// Inert under `[tlsf] repair_mode = "muc"`, which evolves cores rather
    /// than whole specifications.
    ///
    /// It has an on-disk side effect: with the key on, each newly accumulated
    /// specification is written to `<output-dir>/accumulated/` as it is found,
    /// one file per specification, flushed and closed on the spot, so a run
    /// killed by an external wall-clock cap keeps what it had already
    /// accumulated. Those files hold raw gate-passing candidates rather than
    /// the run's filtered output, which stays `repair_N.json` /
    /// `repair_N.tlsf` alone. The directory is created on the first write, so
    /// with the key off nothing is created.
    bool accumulate_repairs = false;
    double selection_rate = 0.5;
    /// Elitism: the top elitism_rate fraction of the population carries over
    /// into the next generation verbatim, bypassing crossover, mutation, and
    /// the offspring filters, so the best candidates are never lost to a
    /// stochastic operator. Must be strictly less than selection_rate (the
    /// elites are a subset of the selected parents).
    ///
    /// Under either NSGA-II scheme this is redundant on paper -- the
    /// (mu+lambda) survivor step already pools parents with offspring and keeps
    /// the best -- and it costs something, because elites bypass the offspring
    /// filter chain, so this fraction of every generation skips the correctness
    /// stages. That costs search pressure rather than output correctness, the
    /// gate in step 5 screening what elites carried in either way (d7733fc).
    /// Both arguments point at 0, and the A/B that
    /// tested them (2026-08-07, 600 paired FRETISH runs and 796 paired TLSF
    /// runs) does not contradict them: quality is non-inferior at 0, with
    /// implies_ideal moving by -0.023 and -0.008 against a +0.05 margin fixed
    /// before launch, and TLSF yield is better at 0 -- 0.746 against 0.714,
    /// McNemar p = 0.0002.
    ///
    /// What keeps it at 0.1 is wall-clock time alone: 0 costs 16.2% more on
    /// TLSF and 8.2% more on FRETISH, against a 10% bound. So the shipped
    /// default trades TLSF yield for speed. Set 0 when finding a repair at all
    /// matters more than finishing quickly.
    ///
    /// The lily02 anecdote this comment used to cite is retired. That campaign
    /// audited every written repair for a guarantee that reduces to `true` and
    /// found them in both arms (3 runs at 0, 4 at 0.1), so they never measured
    /// elitism: each one is `black` answering SAT on the negation of a valid
    /// weak-until formula, which fb4c3ed fixed by rewriting `W` away before
    /// querying it.
    double elitism_rate = 0.1;
    double crossover_rate = 0.1;
    double mutation_rate = 1.0;
    double p_trigger = 0.5;
    double p_response = 0.5;
    double p_timing = 0.15;
    /// Low-probability structural mutation, shared by both modes: append a new
    /// environment assumption (over input atoms) rather than rewriting an
    /// existing requirement/formula. This is how the algorithm can repair
    /// unrealizability that requires strengthening the environment (e.g. adding
    /// a fairness assumption to an unrealizable GR(1) spec), which the
    /// rewrite-only operators cannot express.
    double p_add_assumption = 0.05;
    /// The mirror of p_add_assumption, shared by both modes: delete one
    /// guarantee rather than rewriting it. Some repairs drop a guarantee
    /// outright and are unreachable without this — every `drop-*` ideal in
    /// `examples/` is one, and five TLSF subjects have no other scoreable
    /// ideal. A deleted guarantee is tombstoned in place rather than erased, so
    /// that comparisons against the original stay aligned; see
    /// Requirement::m_removed.
    ///
    /// Defaults to 0.05, matching p_add_assumption: the two are the same move
    /// in opposite directions and there is no reason for the environment to be
    /// easier to strengthen than the system is to relax. Setting it to 0
    /// restores the search exactly as it ran before the operator existed —
    /// the probability is read before the RNG is drawn, so a zero never costs
    /// a draw — which is what reproducing a campaign archived before
    /// 2026-08-13 requires (see the config vintage note in
    /// experiments/README.md).
    ///
    /// The risk to watch is that removal is monotonically good for
    /// realizability, so the status objective pays for it while the similarity
    /// objectives do not, and under NSGA-II a heavily gutted candidate is
    /// non-dominated. TLSF sweep D in scripts/gen_configs.py exists to measure
    /// whether that costs repair quality.
    double p_remove_guarantee = 0.05;
    /// Of the assumptions p_add_assumption appends, the fraction guarded by a
    /// random input atom rather than by `true`. `G F <input>` is strictly
    /// stronger than `G(c -> F <input>)` and so the more powerful repair, which
    /// is why the
    /// unconditional form keeps the majority of the draw.
    double p_conditional_assumption = 0.25;
    /// When true (the default), environment assumptions may reference output
    /// atoms as well as inputs — both a freshly added assumption and a rewrite
    /// of an existing one, on the FRETISH and TLSF paths alike. Setting it
    /// false keeps every assumption input-only and so well-separated by
    /// construction, at the cost of the reactive-environment assumptions
    /// (`G(<output> -> F <input>)`) the wider draw can express. With it on,
    /// what keeps the system from writing itself an assumption it can force to
    /// fail is the well-separation filter rather than the syntactic ban, so
    /// pair it with run_well_separation_filter.
    bool allow_output_assumptions = true;
    /// Mutate assumption timings in the strengthening direction rather than the
    /// weakening one. Weakening the overall assume-guarantee specification
    /// means weakening a guarantee but strengthening an assumption, so
    /// weakening both makes every assumption mutation a move away from a
    /// repair. Retained as a flag only so the two directions can be crossed as
    /// an experiment factor.
    bool strengthen_assumptions = true;
    /// TLSF-mode mutation: probability of mutating an assumption-side section
    /// (INITIALLY/REQUIRE/ASSUME) rather than a guarantee-side one
    /// (PRESET/ASSERT/GUARANTEE) when mutating a tlsf::Specification. The
    /// guarantee side takes the complement. This was once a pair of weights
    /// normalised by their sum, which spent two keys on one degree of freedom.
    double tlsf_p_assumption = 0.3;
    /// TLSF-mode mutation: once a section formula has been chosen for
    /// rewriting, the probability of applying the temporal-structure mutation
    /// (which may insert, drop, or swap X/F/G/U/R/W operators, following
    /// Brizzio et al.) rather than the skeleton-preserving propositional
    /// rewrite. At 0 the temporal skeleton of existing formulae is never
    /// altered.
    double tlsf_p_temporal = 0.2;
    /// TLSF repair strategy (see RepairMode). Muc mode caps its outer
    /// extract-repair-reintegrate loop at muc_max_iterations, so a spec whose
    /// core never becomes realizable ends the run without a repair rather than
    /// looping forever.
    RepairMode repair_mode = RepairMode::Monolithic;
    std::size_t muc_max_iterations = 32;
    std::size_t parallel = std::thread::hardware_concurrency();
    /// Upper bound on ltlsynt processes running concurrently across the whole
    /// program, independent of `parallel`. 0 means unlimited (the default); a
    /// positive value serialises the surplus while the other workers keep doing
    /// non-ltlsynt work.
    ///
    /// Added on the premise that ltlsynt is *the* memory hog; measurement
    /// qualified that. Over 149,153 tool invocations on 2026-08-05, ltlsynt
    /// peaked at 260 MB (21.3 MB mean) against ltlfilt's 3.4 GB and ltl2tgba's
    /// 1.8 GB. On the calls that sample captured, ltlfilt is the hog.
    ///
    /// But that sample cannot bound ltlsynt: it is censored exactly where the
    /// blowups are, since 6 of the heaviest runs died on a 600 s cap before the
    /// atexit report and wrote no profile at all. Rare multi-GB ltlsynt calls
    /// do happen and are absent from those figures. So this stays a safety
    /// valve for a rare tail, and stays unlimited by default because
    /// serialising every realizability query would cost throughput on every run
    /// to bound a few. Set it on a memory-constrained machine. Do not size it
    /// off the figures above -- the max is censored and the tail swings 26x
    /// between seeds on one example.
    std::size_t max_concurrent_realizability = 0;
    /// A fitness function that throws (in practice an external tool failing on
    /// one evolved formula) costs that individual rather than the whole run:
    /// the search is stochastic, so one candidate lost out of a population is
    /// noise, while aborting at generation 23 of 40 loses everything. Above
    /// this fraction of a generation the tooling is broken rather than the
    /// formula, and the run aborts instead of evolving noise into the output.
    /// A single failure is always tolerated, whatever the population size.
    ///
    /// 0.15 rather than 0.05 because the ltl2tgba and ganak budgets now drop
    /// individuals by design: an abandoned call is a lost candidate, and on the
    /// heavy TLSF specifications enough of them land in one generation to trip
    /// a 5% bound. Every archived TLSF campaign raised it to 0.15 for that
    /// reason. It still catches genuinely broken tooling, which fails on
    /// essentially every candidate rather than on a tail of them.
    double max_scoring_failure_rate = 0.15;
};

/// Applies every per-tool timeout in `cfg` to the process-global runner
/// singletons. Drivers must call this rather than setting the tools they happen
/// to remember: compare did the latter and, for as long as the ltlsynt and
/// ltl2tgba budgets had existed, ran both unbounded whatever the config said.
void apply_tool_timeouts(const Config& cfg);
