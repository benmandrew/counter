Configuration
=============

Algorithm parameters — population size, fitness weights, mutation rates and so
on — are tuned without recompiling by passing a *Tom's Obvious Minimal Language*
(TOML) file to ``--config``:

.. code-block:: sh

   counter --input spec.json --output-dir out --config my-config.toml

Every section and key is optional; absent keys keep their built-in defaults.
Sections and keys the parser does not recognise are ignored, each reported on
stderr by its full path, so a misspelling shows up as
``config: unknown key genetic.mutaton_rate, ignoring`` rather than as a setting
that silently did nothing. A key placed in the wrong section is reported the
same way. Values still out of range for their field abort the run.

.. code-block:: toml

   [genetic]
   generations     = 20   # double the default evolution rounds
   population_size = 500

   [runtime]
   parallel = 16              # override thread pool size
   dashboard = true           # stream progress.jsonl + the live dashboard page

``example-config.toml`` in the repository root is an annotated template listing
every key with a comment explaining it. Every value in it **is** that key's
default, so copying the file whole changes nothing;
``scripts/check_config_schema.py`` enforces this against ``include/config.hpp``
as part of the ``lint`` target.

Fitness weights
---------------

Four components are combined into each candidate's score. Semantic similarity
and realisability status dominate; syntactic similarity breaks ties between
semantically comparable candidates, and the Halstead penalty holds back bloat.

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
     - ``ltlsynt`` outcome, mapped to {0, 0.1, 0.2, 0.5, 1.0}
   * - ``fitness.weight_syntactic``
     - 0.2
     - Shared sub-formula count, normalised to [0, 1]
   * - ``fitness.weight_halstead``
     - 0.1
     - Penalty for candidates larger than the original

Semantic similarity is the expensive one: it counts the satisfying traces of a
candidate up to ``model_counting.default_bound`` (default 20) using Ganak over
the transition matrices of SPOT-generated automata. Raising the bound sharpens
the measure and costs time.

Similarity metric
-----------------

``model_counting.metric`` chooses how those trace counts become a score. Both
options take the harmonic mean of two directional containment terms — how much
of each requirement's satisfying traces the other also accepts — and differ
only in that term.

**direct** uses the ratio of counts: the fraction of one requirement's
satisfying traces that also satisfy the other, a Sørensen–Dice overlap. It is a
true overlap measure, but because trace counts grow like ``λ^k``, the ratio
decays toward zero as the bound grows for requirements of differing
permissiveness — so the score depends on ``default_bound``.

**logarithmic** (default) uses the ratio of the counts' *logarithms*, comparing
the languages' growth rates rather than their overlap. That ratio tends to a
constant as the bound grows, so the score is stable across bounds, at the cost
of the direct overlap interpretation. Useful when comparing runs at different
bounds, or when the bound is driven by timing horizons that vary between
candidates.

Selection scheme
----------------

``genetic.selection_scheme`` decides how those four components drive selection.
The two NSGA-II schemes rank identically and are named for the survivor step,
the only thing that differs between them.

.. note::

   These were spelled ``nsga2`` and ``nsga2-replicate`` before 2026-08-06. Both
   spellings are now **rejected** rather than accepted as aliases, so a config
   setting one fails with an error naming its replacement. To reproduce an
   archived campaign, build the commit its ``PROVENANCE.json`` records rather
   than rewriting its config.

**nsga2-truncate** (default) treats them as separate objectives and ranks
candidates by *Non-dominated Sorting Genetic Algorithm II*
(`NSGA-II <https://doi.org/10.1109/4235.996017>`_): Pareto non-domination first,
then crowding distance to spread the population along the front. It searches for
the whole Pareto front — the repairs not beaten on every objective at once —
rather than one weighted compromise. That is useful when the right balance
between, say, semantic similarity and size is not known in advance.

**nsga2-apportion** ranks exactly as ``nsga2-truncate`` does, but changes what
the (μ+λ) survivor step keeps. The pooled parents and offspring are mostly
repeats of a handful of specifications, and duplicates never dominate one
another, so they inflate the rank-0 front until truncating the pool back to
``population_size`` cuts through that front arbitrarily. Deduplicating the pool
on its own is no fix: it leaves a handful of survivors, selection becomes a
no-op, and breeding collapses. Instead the pool is deduplicated, ranked, and —
where the distinct set is smaller than ``population_size`` — replicated back up
to it, weighting each individual by ``1 / (1 + rank)`` with a floor of one copy
and handing out the spare slots by *largest-remainder* (Hamilton)
apportionment. Selection pressure is re-expressed as how many slots an
individual holds rather than whether it survives at all. The step is purely
arithmetic and draws no random numbers, so a seeded run stays reproducible.

The measured effect is diversity. An A/B over the four FRETISH examples at
``generations = 10``, ``population_size = 200`` and ``elitism_rate = 0``, with
20 seeds each for 80 paired runs, raised the distinct candidates held at
generation 9 from 5.9 to 98.3, ``n_repairs`` from 3.43 to 9.94, and the share
of runs finding any repair at all from 0.966 to 1.000 — for roughly 50% more
wall-clock time. Repair quality did not move: pooled ``implies_ideal`` was
0.500 under both schemes. That figure is uninformative here rather than
reassuring, because the corpus sits at its extremes — ``takeoff`` and
``fsm-timing`` score 1.000 either way, ``fsm`` and ``fsm-combined`` score 0.000
either way — so it leaves no headroom in which a quality difference could show.
The diversity gain is measured; the quality gain is unproven.

**weighted** collapses them into a single weighted average and ranks by that
scalar, using truncation selection with elitism. In principle this finds the one
repair that best fits the configured trade-off; in practice it converges
prematurely and then stagnates. Over a 50k-run parameter sweep its results did
not move with the generation count at any level from 5 to 80, and on the
``takeoff`` example it matched an ideal repair in 1.7% of runs against
``nsga2-truncate``'s 89.3%, at no saving in wall-clock time. It is kept for
comparison rather than for use.

Two consequences are worth knowing. Under either NSGA-II scheme the
``[fitness]`` weights only decide which components are active (weight > 0);
they no longer bias selection. And their survivor selection pools each
generation's parents with their offspring and keeps the best — a (μ+λ) scheme,
already elitist — so ``elitism_rate = 0`` is the natural companion setting.

All three schemes still emit the weighted-average scalar in each repair's
fitness record, so outputs stay comparable across runs.

Filters
-------

``[filters]`` toggles the per-generation and final filters; each runs every
generation when enabled.

``run_weakening`` keeps only repairs the original logically implies — the
genuine weakenings. It is a **final screen** over the realizable survivors
rather than a per-generation filter, and is on by default.

It was moved out of the per-generation set on measurement. The ``cj-large``
campaign is the only one to cross the factor, over 9,796 paired runs: filtering
each generation lost 1,005 of them and won 410, never helped on any of the four
specs, and cost 20 points of implies-ideal on ``fsm`` (0.563 → 0.360) — buying
a 26% wall-time saving with repair quality. Screening only the final population
leaves the search bit-identical while still guaranteeing that a written repair
does not forbid behaviour the original allowed.

The screen applies on both paths, but the implication check behind it differs
in strength. On FRETISH, ``spec_implies`` decomposes the assume-guarantee pair
and matches each requirement against a *single* counterpart, so it under-detects
— an implication that only holds via several requirements together is missed,
and a genuine weakening can be rejected. On TLSF, ``tlsf_spec_implies`` lowers
the whole specification to one LTL formula and asks ``black`` whether
``(original) & !(candidate)`` is unsatisfiable, which is exact.

Expect the TLSF screen to reject more, and to be right when it does. Nothing
constrains mutation to weaken: it can delete a safety guarantee and add a
stronger one, reaching realizability while forbidding behaviour the original
allowed. The MUC repair of the two-client arbiter fixture does exactly this —
it drops the mutex ``G !(g0 & g1)`` and adds ``G g1`` — and is rejected. Turning
``run_weakening`` off on a TLSF run will therefore produce more written repairs,
not better ones.

``run_well_separation`` drops candidates that are not *well-separated*: ones the
system can satisfy vacuously by forcing its own assumptions to fail.
Realizability is decided on ``(assumptions) -> (guarantees)``, so replacing the
guarantees with ``false`` and finding ``(assumptions) -> false`` realizable
means the system has a strategy that breaks the assumptions on its own. That is
strictly stronger than ``run_vacuity``. It is on by default.

It is the counterpart to ``mutation.allow_output_assumptions``, which also
defaults on. That flag lets an environment assumption reference output atoms;
this filter is what then stops the search writing the system an assumption it
can defeat. The two are meant to move together, and turning this off while
leaving that on is the one combination with no guard.

Unlike ``run_weakening`` it is a per-generation filter rather than a final
screen, and that is deliberate. A 7,200-run TLSF campaign measured an
end-of-run pass leaking 42.7% not-well-separated repairs against 44.1% with the
filter off entirely — indistinguishable — because the elites bypass the filter
chain and NSGA-II pools every parent unfiltered, so a late pass's drops are
re-admitted. ``run_weakening`` escapes that only because its screen sits over
the collected realizable survivors, downstream of both.

This filter is the one caller that reads a failed synthesis the other way
round. Everywhere else an undecided ``ltlsynt`` query means "do not admit this
repair", so falling back on *unrealizable* is the cautious answer; here
unrealizable is what *keeps* a candidate, so the same fallback would admit
specifications nobody checked — and it is this filter's own query load that
makes ``runtime.ltlsynt_timeout_ms`` fire in the first place. An undecided
query therefore drops the candidate.

The undecided outcome is memoised, so the candidate stays dropped for the rest
of the run without re-running ``ltlsynt``. That is deliberate rather than a
leftover of the old behaviour: ``ltlsynt`` is deterministic and its call
durations are sharply bimodal --- 95% of the TLSF campaign's calls finished
under 50 ms, the 0.5--1 s band was almost empty, and a 500 ms cap abandoned
within 0.1% of the calls a 10 s cap did --- so a formula that blows the budget
is in the minutes-long tail rather than near the boundary, and asking again
buys the same non-answer at full price. What caching a plain ``false`` used to
do, and this does not, is decide the question: ``nullopt`` is not a verdict, so
every caller still resolves it in its own direction on every cache hit.

Each test is a full ``ltlsynt`` query, run only where an assumption references
an output atom, so it costs nothing on specifications whose assumptions are
input-only. It was off by default until that campaign priced it: filtering
every generation came out about 5% *faster* than not filtering, because a
candidate dropped before the scoring stage never costs a model-count or a
synthesis query.

The per-filter run intervals that once throttled these were removed: across
every archived campaign not one config had ever set them.

Runtime
-------

``runtime.parallel`` overrides the thread pool size, which otherwise follows
``std::thread::hardware_concurrency()``. ``runtime.black_timeout_ms`` bounds
each ``black`` satisfiability query, defaulting to 1000 ms.

``runtime.dashboard`` streams per-stage and per-generation progress to
``<output-dir>/progress.jsonl`` and copies the live dashboard page beside it, so
``python3 -m http.server -d <output-dir> 8000`` shows the run as it happens. It
defaults to false, because a campaign of many runs pays for the extra file and
its flushes with nobody watching; ``counter --dashboard`` enables it for a
single run without editing a config. The flag can only turn the dashboard on —
a config that already asked for it is not disabled by omitting the flag.

``counter --cpu-report`` prints a CPU-attribution report at the end, separating
time spent in counter's own code from time spent in the external tools
(``black``, ``ltlsynt``, ``ganak``), measured per-process via
``getrusage``/``wait4``. It is a flag rather than a config key because it asks
about one interactive run rather than about the search, as ``COUNTER_PROFILE``
does for the scope profiler.
