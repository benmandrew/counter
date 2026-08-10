Configuration
=============

Algorithm parameters — population size, fitness weights, mutation rates and so on — are tuned without recompiling by passing a *Tom's Obvious Minimal Language* (TOML) file to ``--config``:

.. code-block:: sh

   counter --input spec.json --output-dir out --config my-config.toml

Every section and key is optional; absent keys keep their built-in defaults. Sections and keys the parser does not recognise are ignored, each reported on stderr by its full path, so a misspelling shows up as ``config: unknown key genetic.mutaton_rate, ignoring`` rather than as a setting that silently did nothing. A key placed in the wrong section is reported the same way. Values still out of range for their field abort the run.

.. code-block:: toml

   [genetic]
   generations     = 20   # double the default evolution rounds
   population_size = 500

   [runtime]
   parallel = 16              # override thread pool size
   dashboard = true           # stream progress.jsonl + the live dashboard page

``example-config.toml`` in the repository root is an annotated template listing every key with a comment explaining it. Every value in it **is** that key's default, so copying the file whole changes nothing; ``scripts/check_config_schema.py`` enforces this against ``include/config.hpp`` as part of the ``lint`` target.

Fitness weights
---------------

Four components are combined into each candidate's score. Semantic similarity and realisability status dominate; syntactic similarity breaks ties between semantically comparable candidates, and the Halstead penalty holds back bloat.

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Key
     - Default
     - Component
   * - ``fitness.weight_semantic``
     - 0.5
     - Bounded model counting of satisfying LTL traces
   * - ``fitness.weight_status``
     - 0.5
     - ``black`` + ``ltlsynt`` outcome, mapped to {0, 0.5, 1.0}
   * - ``fitness.weight_syntactic``
     - 0.2
     - Shared sub-formula count, normalised to [0, 1]
   * - ``fitness.weight_halstead``
     - 0.1
     - Penalty for candidates larger than the original

Semantic similarity is the expensive one: it counts the satisfying traces of a candidate up to ``model_counting.default_bound`` (default 20) using Ganak over the transition matrices of SPOT-generated automata. Raising the bound sharpens the measure and costs time.

The status scale is three points and coarse on purpose. 0 means some component formula of the candidate is unsatisfiable on its own; 1.0 means the candidate is realizable and *well-separated* with it; 0.5 is everything in between — either no strategy exists, or the only strategy is to force the candidate's own assumptions to fail. Those last two share a tier rather than being graded apart, because a tier between them would rank cheating above failing. Ill-separation is cheap for mutation to reach and expensive to leave, so an intermediate score would be a broad plateau the search settles on — the shape the aggregate already shows for gutted vacuous repairs. Scoring it level with unrealizable keeps such a candidate in the population as breeding material, which is the value of not dropping it, and pays it nothing for the cheat. Grading *within* unrealizability instead needs a measure of how far a candidate sits from realizable, which a satisfiability query cannot express.

Both front ends fold the well-separation query in behind the realizability one, so it is asked only where it can change the answer: a candidate already unrealizable cannot be realizable for the wrong reason. The top tier only carries that meaning while ``filters.run_well_separation`` is off, which is its default and for this reason; see the filter below.

Similarity metric
-----------------

``model_counting.metric`` chooses how those trace counts become a score. Both options take the harmonic mean of two directional containment terms — how much of each requirement's satisfying traces the other also accepts — and differ only in that term.

**direct** uses the ratio of counts: the fraction of one requirement's satisfying traces that also satisfy the other, a Sørensen–Dice overlap. It is a true overlap measure, but because trace counts grow like ``λ^k``, the ratio decays toward zero as the bound grows for requirements of differing permissiveness — so the score depends on ``default_bound``.

**logarithmic** (default) uses the ratio of the counts' *logarithms*, comparing the languages' growth rates rather than their overlap. That ratio tends to a constant as the bound grows, so the score is stable across bounds, at the cost of the direct overlap interpretation. Useful when comparing runs at different bounds, or when the bound is driven by timing horizons that vary between candidates.

Selection scheme
----------------

``genetic.selection_scheme`` decides how those four components drive selection. The two NSGA-II schemes rank identically and are named for the survivor step, the only thing that differs between them.

.. note::

   These were spelled ``nsga2`` and ``nsga2-replicate`` before 2026-08-06. Both spellings are now **rejected** rather than accepted as aliases, so a config setting one fails with an error naming its replacement. To reproduce an archived campaign, build the commit its ``PROVENANCE.json`` records rather than rewriting its config.

**nsga2-truncate** (default) treats them as separate objectives and ranks candidates by *Non-dominated Sorting Genetic Algorithm II* (`NSGA-II <https://doi.org/10.1109/4235.996017>`_): Pareto non-domination first, then crowding distance to spread the population along the front. It searches for the whole Pareto front — the repairs not beaten on every objective at once — rather than one weighted compromise. That is useful when the right balance between, say, semantic similarity and size is not known in advance.

**nsga2-apportion** ranks exactly as ``nsga2-truncate`` does, but changes what the (μ+λ) survivor step keeps. The pooled parents and offspring are mostly repeats of a handful of specifications, and duplicates never dominate one another, so they inflate the rank-0 front until truncating the pool back to ``population_size`` cuts through that front arbitrarily. Deduplicating the pool on its own is no fix: it leaves a handful of survivors, selection becomes a no-op, and breeding collapses. Instead the pool is deduplicated, ranked, and — where the distinct set is smaller than ``population_size`` — replicated back up to it, weighting each individual by ``1 / (1 + rank)`` with a floor of one copy and handing out the spare slots by *largest-remainder* (Hamilton) apportionment. Selection pressure is re-expressed as how many slots an individual holds rather than whether it survives at all. The step is purely arithmetic and draws no random numbers, so a seeded run stays reproducible.

The measured effect is diversity. An A/B over the four FRETISH examples at ``generations = 10``, ``population_size = 200`` and ``elitism_rate = 0``, with 20 seeds each for 80 paired runs, raised the distinct candidates held at generation 9 from 5.9 to 98.3, ``n_repairs`` from 3.43 to 9.94, and the share of runs finding any repair at all from 0.966 to 1.000 — for roughly 50% more wall-clock time. Repair quality did not move: pooled ``implies_ideal`` was 0.500 under both schemes. That figure is uninformative here rather than reassuring, because the corpus sits at its extremes — ``takeoff`` and ``fsm-timing`` score 1.000 either way, ``fsm`` and ``fsm-combined`` score 0.000 either way — so it leaves no headroom in which a quality difference could show. The diversity gain is measured; the quality gain is unproven.

That A/B is FRETISH, and the picture on TLSF is different. The ``2026-07-24-ablation`` campaign crossed the two schemes over 21 TLSF specifications at 80 seeds each, 3,076 runs, and they split the two measures between them:

.. list-table::
   :header-rows: 1
   :widths: 34 22 22 22

   * - Measure
     - ``nsga2``
     - ``weighted``
     - Winner
   * - Found any repair
     - 0.764
     - 0.868
     - ``weighted``
   * - Matched an ideal (all runs)
     - 0.245
     - 0.148
     - ``nsga2``
   * - Matched an ideal (runs that found one)
     - 0.321
     - 0.170
     - ``nsga2``

Per specification the yield difference is narrow but lopsided where it lands: 15 ties, 4 wins for ``weighted``, 1 for ``nsga2``. Two of those wins are total. On ``arbiter`` — the GR(1) two-client arbiter, whose repair needs a fairness assumption added — ``nsga2`` finds nothing in 80 runs and ``weighted`` finds a repair in all 80, matching an ideal in 31 of them. On ``rg1`` the counts are 4 of 80 against 80 of 80. Both still hold at the current default: on ``arbiter`` at seed 42, ``nsga2-truncate`` returns no repair and pins at fitness 0.807692, while ``weighted`` returns 22 realisable survivors and 6 maximal repairs at 0.928367.

So the schemes are not ordered on TLSF, they trade. ``weighted`` finds a repair more often; ``nsga2`` finds the *right* repair about twice as often, on either denominator, and on ``lily02`` the gap is stark — both schemes repair it in nearly every run, but ``nsga2`` matches an ideal 80 times out of 80 against ``weighted``'s 39. Treat a TLSF specification that yields nothing under the default as a candidate for ``weighted`` rather than as unrepairable, and read what comes back with the quality figures above in mind.

**weighted** collapses them into a single weighted average and ranks by that scalar, using truncation selection with elitism. In principle this finds the one repair that best fits the configured trade-off; in practice it converges prematurely and then stagnates. Over a 50k-run parameter sweep its results did not move with the generation count at any level from 5 to 80, and on the ``takeoff`` example it matched an ideal repair in 1.7% of runs against ``nsga2-truncate``'s 89.3%, at no saving in wall-clock time. On the FRETISH corpus it is kept for comparison rather than for use; on TLSF it is the scheme of last resort for a specification the default cannot repair at all, at the cost of repair quality noted above.

Two consequences are worth knowing. Under either NSGA-II scheme the ``[fitness]`` weights only decide which components are active (weight > 0); they no longer bias selection. And their survivor selection pools each generation's parents with their offspring and keeps the best — a (μ+λ) scheme, already elitist — so ``elitism_rate = 0`` is the natural companion setting.

All three schemes still emit the weighted-average scalar in each repair's fitness record, so outputs stay comparable across runs.

Search size and operators
-------------------------

``[genetic]`` sizes the search. ``generations`` and ``population_size`` are the two that matter. ``selection_rate`` (0.5) is the share of the population that breeds, and ``elitism_rate`` (0.1) the share carried into the next generation verbatim, bypassing crossover, mutation and the offspring filters. Elites are a subset of the parents, so ``elitism_rate`` must stay strictly below ``selection_rate``. ``crossover_rate`` (0.1) and ``mutation_rate`` (1.0) are the per-offspring probabilities of applying each operator.

``elitism_rate`` looks redundant under the default scheme and is kept anyway. Under either NSGA-II scheme the (μ+λ) survivor step already pools parents with offspring and keeps the best, so an elite carry-over on top adds nothing in principle — and it is not free, because elites bypass the offspring filter chain, so that fraction of every generation skips ``run_vacuity`` and ``run_well_separation``. Both arguments say 0.

The one measurement taken says otherwise. Dropping it to 0 on ``examples/lily02`` at seed 42 leaves the weighted scalar a shade higher (best 0.954 against 0.950) but returns structurally weaker repairs, one of which — ``req W (F(G cancel | G true))`` — reduces to ``true`` and so deletes the guarantee outright, where 0.1 returns three structured rewrites of it. That repair passes every screen honestly: deleting a guarantee *is* a weakening, and a weaker specification is implication-maximal. Aggregate fitness prefers a gutted specification and nothing on the TLSF path screens one out, so elitism appears to be acting as a brake on that pathology rather than as dead weight. One example is not a result; treat this as a reason not to change the default on the redundancy argument alone.

``[mutation]`` weights what a mutation does. ``p_trigger`` (0.5), ``p_response`` (0.5) and ``p_timing`` (0.15) select which part of a FRETISH requirement is rewritten.

``p_add_assumption`` (0.05) is the structural operator shared by both paths: rather than rewriting an existing requirement, it appends a new environment assumption. It is the only way the search can repair an unrealisability that needs the environment strengthened — adding a fairness assumption to an unrealisable *generalised reactivity of rank 1* (GR(1)) specification, say. The rewrite-only operators cannot express that move. Of the assumptions it appends, ``p_conditional_assumption`` (0.25) is the fraction guarded by a random input atom rather than by ``true``; ``G F <input>`` is strictly stronger than ``G(c -> F <input>)`` and so the more effective repair, which is why the unconditional form keeps most of the draw.

``allow_output_assumptions`` (on) lets an assumption reference output atoms as well as inputs. It buys the reactive-environment assumptions ``G(<output> -> F <input>)`` that an input-only draw cannot express. Stopping the system from writing itself an assumption it can defeat is then the status score's job rather than a syntactic ban's: an assumption side the system can force to fail costs the candidate the top status tier, and the output gate refuses to write it whatever the filters say.

``strengthen_assumptions`` (on) mutates assumption timings in the strengthening direction. Weakening the overall assume-guarantee pair means weakening a guarantee but *strengthening* an assumption. Weakening both would therefore make every assumption mutation a move away from a repair. It is a flag only so the two directions can be crossed as an experiment factor.

TLSF mode
---------

``[tlsf]`` applies on the TLSF path alone and is ignored on the FRETISH one.

``repair_mode`` chooses the repair strategy. **monolithic** (the default) evolves the whole specification at once, exactly as the FRETISH path does. **muc** instead extracts a *minimal unrealisable core* and evolves only that sub-specification. It then reintegrates the repaired core with the untouched non-core guarantees, and repeats on the recombined specification until it is realisable. ``muc_max_iterations`` (32) caps that outer loop, so a specification whose core never becomes realisable ends the run without a repair rather than looping forever. :doc:`tlsf` describes the mode and the ``mucs`` tool that exposes the same extraction on its own.

``[tlsf.mutation]`` tunes the TLSF operators. ``p_assumption`` (0.3) is the probability of mutating an assumption-side section (``INITIALLY``, ``REQUIRE``, ``ASSUME``) rather than a guarantee-side one (``PRESET``, ``ASSERT``, ``GUARANTEE``); the guarantee side takes the complement. ``p_temporal`` (0.2) is the probability that a chosen formula gets the temporal-structure mutation — inserting, dropping or swapping ``X``/``F``/``G``/``U``/``R``/``W`` — rather than the skeleton-preserving propositional rewrite. At ``p_temporal = 0`` the temporal skeleton of an existing formula is never altered.

Filters
-------

``[filters]`` toggles the filters. Most run once per generation, before scoring, so a dropped candidate never costs a model-count or a synthesis query; ``run_weakening`` is the exception and is described first.

``run_implication`` keeps only the specifications that are maximal under the implication partial order, discarding any repair another repair already subsumes. Like ``run_weakening`` it is a final screen rather than a per-generation filter, and it is on by default. ``run_vacuity`` drops candidates that hold for free rather than because anything was repaired, and is the weaker relative of ``run_well_separation`` below. Three tests, under one flag and one ``vacuity`` stage, applied cheapest first.

A **syntactic screen** rejects a requirement whose condition is the literal ``false``, in an assumption or a guarantee. It costs no solver call, and the condition sits only in the antecedent of the lowered implication, so it is vacuous under every timing. There is deliberately no dual over responses: a ``true`` response is a no-op under most timings but not under ``after n ticks``, whose lowering negates the response and so makes ``true`` the strongest guarantee expressible. Only the check below reads the lowered formula, so only it gets that case right. On TLSF, which has no condition/response split and so no such lowering, the screen reads a ``false`` formula in INITIALLY/REQUIRE/ASSUME or a ``true`` formula in PRESET/ASSERT/GUARANTEE.

A **guarantee validity check** then negates each guarantee individually and rejects the candidate on the first one whose negation is unsatisfiable — a guarantee that demands nothing. This is the case ``run_weakening`` is least able to catch, since the original implies a valid guarantee trivially and a gutted guarantee therefore reads as a perfect weakening. Per guarantee rather than over the guarantee conjunction: the conjunction is valid only when *every* guarantee is, so it would keep a candidate that guts one conjunct while the rest still constrain the system.

An **assumption satisfiability check** asks whether the assumptions are *jointly unsatisfiable*: a false antecedent makes ``(assumptions) -> (guarantees)`` a tautology, so such a candidate is realizable for free and is not a repair — and ``run_weakening`` cannot catch it either, because unsatisfiable assumptions imply every other assumption and so pass every implication test. This one stays a single joint query. Satisfiability does not distribute over conjunction: ``G p`` and ``G !p`` are each satisfiable and jointly are not.

The guarantee queries are cached per requirement, so a guarantee carried over from a parent — and every locked one — costs nothing after its first evaluation; the assumption query is cached per assumption side, and costs nothing at all on a specification with no assumptions. Every solver verdict is conservative in the same direction: a timeout reads as non-vacuous, so no candidate is dropped on a non-answer.

It is on by default, and what turning it off does is what turning any correctness flag off does. The flags decide whether a property is enforced *per generation*, and nothing more: collecting the realizable survivors applies every row of the shared correctness table (``include/filter/correctness.hpp``), on both paths, whichever flags are set. So a flag set to ``false`` relaxes search pressure rather than admitting the repairs it screens.

``run_weakening`` keeps only repairs the original logically implies — the genuine weakenings. It is a **final screen** over the realizable survivors rather than a per-generation filter, and is on by default.

It was moved out of the per-generation set on measurement. The ``cj-large`` campaign is the only one to cross the factor, over 9,796 paired runs: filtering each generation lost 1,005 of them and won 410, never helped on any of the four specs, and cost 20 points of implies-ideal on ``fsm`` (0.563 → 0.360) — buying a 26% wall-time saving with repair quality. Screening only the final population leaves the search bit-identical while still guaranteeing that a written repair does not forbid behaviour the original allowed.

The screen applies on both paths, but the implication check behind it differs in strength. On FRETISH, ``spec_implies`` decomposes the assume-guarantee pair and matches each requirement against a *single* counterpart, so it under-detects — an implication that only holds via several requirements together is missed, and a genuine weakening can be rejected. On TLSF, ``tlsf_spec_implies`` lowers the whole specification to one LTL formula and asks ``black`` whether ``(original) & !(candidate)`` is unsatisfiable, which is exact.

Expect the TLSF screen to reject more, and to be right when it does. Nothing constrains mutation to weaken: it can delete a safety guarantee and add a stronger one, reaching realizability while forbidding behaviour the original allowed. The MUC repair of the two-client arbiter fixture does exactly this — it drops the mutex ``G !(g0 & g1)`` and adds ``G g1`` — and is rejected. Turning ``run_weakening`` off on a TLSF run will therefore produce more written repairs, not better ones.

``run_well_separation`` drops candidates that are not *well-separated*: ones the system can satisfy vacuously by forcing its own assumptions to fail. Realizability is decided on ``(assumptions) -> (guarantees)``, so replacing the guarantees with ``false`` and finding ``(assumptions) -> false`` realizable means the system has a strategy that breaks the assumptions on its own. That is strictly stronger than ``run_vacuity``. It is **off** by default.

The default is forced by the ordering rather than chosen. Well-separation is part of the status score now: the top tier means realizable *and* well-separated, so a candidate the system can satisfy only by defeating its own assumptions scores 0.5 and the search ranks it down instead of losing it. Filters run before scoring, so leaving this one on drops that candidate before anything scores it, and the tier never fires. An earlier assumption-side status tier sat unreachable behind ``run_vacuity`` for exactly this reason — measured over 887k scored candidates, found to hold none of them, and removed for it in commit ``a64f833``.

It remains the counterpart to ``mutation.allow_output_assumptions``, which defaults on and lets an environment assumption reference output atoms. What stops the search writing the system an assumption it can defeat is the status tier during the search and the output gate at the end, rather than this filter. Turning the filter on adds a guard and disables the first of those.

Turning it on means a per-generation filter rather than a final screen, and that part is settled. A 7,200-run TLSF campaign measured an end-of-run pass leaking 42.7% not-well-separated repairs against 44.1% with the filter off entirely — indistinguishable — because the elites bypass the filter chain and NSGA-II pools every parent unfiltered, so a late pass's drops are re-admitted. ``run_weakening`` escapes that only because its screen sits over the collected realizable survivors, downstream of both.

Two arrangements both sound like "filtering at the end", and only one of them was measured. What that campaign priced was a per-generation *stage* restricted to the last generation, which elites and parent pooling defeat exactly as above. The *gate* over the collected realizable survivors is the other, and it sits downstream of both — the same position that makes ``run_weakening`` work. The gate applies well-separation unconditionally, so a not-well-separated specification is never written out whatever this flag says. Output correctness does not turn on the flag in either direction; search pressure is the whole of what it decides.

The filter is the one caller that reads a failed synthesis the other way round. Everywhere else an undecided ``ltlsynt`` query means "do not admit this repair", so falling back on *unrealizable* is the cautious answer; here unrealizable is what *keeps* a candidate, so the same fallback would admit specifications nobody checked — and it is this query's own load that makes ``runtime.ltlsynt_timeout_ms`` fire in the first place. An undecided query therefore drops the candidate.

The undecided outcome is memoised, so the candidate stays dropped for the rest of the run without re-running ``ltlsynt``. That is deliberate rather than a leftover of the old behaviour: ``ltlsynt`` is deterministic and its call durations are sharply bimodal --- 95% of the TLSF campaign's calls finished under 50 ms, the 0.5--1 s band was almost empty, and a 500 ms cap abandoned within 0.1% of the calls a 10 s cap did --- so a formula that blows the budget is in the minutes-long tail rather than near the boundary, and asking again buys the same non-answer at full price. What caching a plain ``false`` used to do, and this does not, is decide the question: ``nullopt`` is not a verdict, so every caller still resolves it in its own direction on every cache hit.

Each test is a full ``ltlsynt`` query, run only where an assumption references an output atom, so it costs nothing on specifications whose assumptions are input-only. The same campaign priced turning it on at about 5% *faster* than not filtering, because a candidate dropped before the scoring stage never costs a model-count or a synthesis query. That is the trade the default takes the other side of: the wall time buys nothing the gate does not already guarantee, and costs the search the gradient off ill-separation.

The per-filter run intervals that once throttled these were removed: across every archived campaign not one config had ever set them.

Runtime
-------

``runtime.parallel`` overrides the thread pool size, which otherwise follows ``std::thread::hardware_concurrency()``.

``runtime.max_concurrent_realizability`` caps how many ``ltlsynt`` processes run at once across the whole program, independently of ``parallel``. The default, 0, means unlimited; a positive value serialises the surplus while the other workers carry on with non-``ltlsynt`` work.

It was added on the premise that ``ltlsynt`` is *the* memory hog, and measurement qualified that. Over 149,153 tool invocations across the corpus, ``ltlsynt`` peaked at 260 MB, against 1.8 GB for ``ltl2tgba`` and 3.4 GB for ``ltlfilt`` — which is 51% of all invocations and has by far the worst tail. On the calls that sample captured, ``ltlfilt`` is the hog and ``ltlsynt`` is not.

What that sample cannot do is bound ``ltlsynt``. It is censored precisely where the blowups live: six of the heaviest runs (``amba``, ``full-arbiter``, ``prioritized-arbiter``) were killed on a 600 s wall-clock cap, skipped the ``atexit`` report and wrote no profile at all. Rare multi-gigabyte ``ltlsynt`` calls do happen, and none of them are in those figures. Treat 260 MB as what the common case costs, not as a ceiling.

So the cap stays a real safety valve for a rare tail rather than a defence against the typical call, and it stays unlimited by default: the event is infrequent enough that serialising every realizability query against it would cost throughput on every run to bound a few. Set it on a memory-constrained machine, or on a corpus with the heavy SYNTCOMP arbiters in it. Do not size it off the figures above — the maximum is censored, and on one example the tail moved 26x between two seeds. Bounding ``ltlfilt`` concurrency is the lever for the *common* case, and there is no key for it yet.

``runtime.max_scoring_failure_rate`` (0.15) bounds what fraction of a generation may fail to score before the run aborts. A fitness function that throws — in practice an external tool failing on one evolved formula — costs that individual rather than the whole run. The search is stochastic, so one lost candidate out of a population is noise, whereas aborting at generation 23 of 40 loses everything. Above this fraction the tooling is broken rather than the formula, and continuing would only evolve noise into the output.

Per-tool budgets
~~~~~~~~~~~~~~~~

Each external tool has a per-call wall-clock budget in milliseconds. A budget of 0 means no timeout.

.. list-table::
   :header-rows: 1
   :widths: 34 12 54

   * - Key
     - Default
     - On expiry
   * - ``runtime.black_timeout_ms``
     - 1000
     - The satisfiability query is undecided.
   * - ``runtime.ltlfilt_timeout_ms``
     - 10000
     - The formula goes unsimplified.
   * - ``runtime.ltlsynt_timeout_ms``
     - 500
     - The realisability query is undecided.
   * - ``runtime.ltl2tgba_timeout_ms``
     - 60000
     - The individual is dropped.
   * - ``runtime.ganak_timeout_ms``
     - 0 (off)
     - The individual is dropped.

What decides whether a budget defaults on is the cost of abandoning a call weighed against the cost of not doing so. ``black`` has its own internal timeout, so bounding it is routine. ``ltlfilt`` is bounded because ``--simplify`` is super-exponential on the deep nested-``X`` conjunctions the search builds, and an abandoned call there costs only a missed simplification, never a candidate.

``ltlsynt`` and ``ltl2tgba`` are bounded despite an abandoned call costing a candidate, because an unbounded one can cost the whole run: ``ltlsynt`` occasionally runs for minutes with no upper limit, and ``ltl2tgba``'s deterministic construction has the same super-exponential blowup as ``ltlfilt``, running for hours and leaking orphaned multi-gigabyte processes over a long campaign. Losing one candidate out of a population is the noise ``max_scoring_failure_rate`` already exists to absorb; losing the run at generation 23 of 40 is not. Both defaults are what every archived TLSF campaign set explicitly, which is the other half of the argument — the shipped default should be the configuration real runs use.

The two budgets differ by two orders of magnitude because the tools' distributions do. ``ltlsynt`` call durations are sharply bimodal: 95% of a TLSF campaign's calls finished under 50 ms, the 0.5–1 s band was almost empty, and a 500 ms cap abandoned within 0.1% of the calls a 10 s cap did — so a query that blows 500 ms is in the minutes-long tail rather than near the boundary. A legitimate ``ltl2tgba`` determinisation, by contrast, can genuinely take tens of seconds, so its budget is set to bound the blowup rather than to discriminate a tail.

``ganak`` stays off. Counting is the fitness function's real work, so a slow count is usually a legitimately hard one rather than a blowup, and there is no equivalent runaway to bound.

.. note::

   A wall-clock budget makes a run's output depend on the machine it ran on. A slower host abandons a call that a faster one completes, so the same ``--seed`` can give different repairs on different hardware — and a loaded machine differs from an idle one. The seed fixes the search; the budgets do not. When reproducing a result exactly, match the hardware, and read the ``tool_calls`` timeout counts in ``<output-dir>/run.json``, which every run that completes writes regardless of any reporting flag, to tell a genuine difference from an abandoned call.

An undecided ``ltlsynt`` answer is resolved by each caller in its own direction — no repair is admitted on one, and the well-separation filter drops the candidate, for the reason given above. A dropped individual counts against ``max_scoring_failure_rate``.

Reporting
~~~~~~~~~

``runtime.dashboard`` streams per-stage and per-generation progress to ``<output-dir>/progress.jsonl`` and copies the live dashboard page beside it, so ``python3 -m http.server -d <output-dir> 8000`` shows the run as it happens. It defaults to false, because a campaign of many runs pays for the extra file and its flushes with nobody watching; ``counter --dashboard`` enables it for a single run without editing a config. The flag can only turn the dashboard on — a config that already asked for it is not disabled by omitting the flag.

``counter --cpu-report`` prints a CPU-attribution report at the end, separating time spent in counter's own code from time spent in the external tools (``black``, ``ltlsynt``, ``ganak``), measured per-process via ``getrusage``/``wait4``. It is a flag rather than a config key because it asks about one interactive run rather than about the search, as ``COUNTER_PROFILE`` does for the scope profiler.

``counter --diagnostics`` prints the engine-internal counters at exit: the per-tool call, time and cache table, the *constant-folded* count — satisfiability questions ``ltlfilt`` decided outright, with no ``black`` call — the ``ltl2tgba`` tautology-substitution count, printed only when it is non-zero, which counts the formulae the counting path replaced with the universal automaton to work around a SPOT bug that exits 2 on a tautology, and the fitness cache hit rate. Every one of them printed on every run until they were put behind this flag. They are off by default because stdout is for watching a run in progress, and none of them says anything about the run's repairs. Nothing is lost by that default: every figure in the report is written to ``<output-dir>/run.json`` whether or not the flag is given, and a campaign should read them from there, since parsing a table out of a log is worse than reading the JSON. It is a flag rather than a config key for the same reason ``--cpu-report`` is — it asks about one interactive run rather than about the search.

Left to itself, stdout carries the run rather than a report: a status line per generation, then the filter report, the scoring report and a closing ``Done in <n>s``. Keeping it to that is also why ``counter`` rejects an argument it does not recognise instead of ignoring it. The ones that catch people name real config keys — ``--generations`` and ``--population_size`` are settings rather than flags, and passing them on the command line used to run an entire search against the defaults without a word about it.

The run manifest
~~~~~~~~~~~~~~~~

Every run that completes writes a *run manifest* to ``<output-dir>/run.json``, so an output directory names its own inputs. Nothing in these docs described it until now, while two passages above already sent the reader off to read it.

It records ``schema_version`` (3 at present), ``tool``, the build's ``commit``, ``commit_short`` and ``dirty``, then ``finished_utc``, ``wall_s``, ``input``, ``seed``, ``n_repairs`` and ``input_screen``. That last one is null when the input specification passed every correctness check at load time, and otherwise names the check it failed. ``config`` holds the fully merged configuration in TOML section layout, so it diffs directly against a config file. ``tool_calls`` holds ``calls``, ``cache_hits``, ``timeouts`` and ``total_s`` per tool. The engine counters follow: ``n_constant_folded``, ``n_tautology_substitutions``, and ``fitness_cache``, itself a pair of ``hits`` and ``misses``.

``schema_version`` went from 1 to 2 when those last three were added, at the point the counters stopped printing to stdout by default and the manifest became their only unconditional record. The rule that falls out of that is worth stating: a counter added to the ``--diagnostics`` report has to be added to the manifest as well, or turning the flag off loses it. A figure that reaches stdout alone is a figure the next default can retire without anyone noticing.

2 to 3 added ``input_screen``, and the reason is the same one in a different guise. The load-time screen warns on stdout, so a campaign that wanted to know which of its runs started from an ill-formed input had to grep the log for that warning; the manifest field lets it partition the runs on a key it already reads. Both bumps say that anything a run learns about itself belongs in the file, not only in the output stream.
