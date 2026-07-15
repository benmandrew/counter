# Experiment plan — the weakening filter as a crossed factor

The 2026-07-14 factorial answered sweeps A (*generations*) and B (*population
size*) well and sweeps C–J badly. A and B both improve the outcome metric
monotonically, so the gen10/pop200 point that C–J were all measured at sits
near the bottom of both curves. Every C–J result is a reading taken in the
regime where the genetic algorithm (GA) performs worst.

The largest effect anywhere in C–J was sweep J, the *weakening filter*: turning
it **off** scored 0.575 against 0.500 for on. That is a filter improving results
by being disabled, measured at the worst operating point, with no indication of
whether it survives at scale or how it interacts with anything else.

This plan therefore promotes `run_weakening` from a sweep to a **crossed
factor**. Sweeps C, D, E, F and I run at generations=40 and population_size=1000
under NSGA-II, each level executed twice — once with weakening on, once off —
for 90 seeds. Estimated cost is **16.0 hours wall-clock** across av2 and av3 in
parallel, against an 18-hour budget.

## What the existing data says

All figures are NSGA-II rows from `experiments-14-07-2026/results-factorial.csv`
(25,196 rows). The metric is `implies_ideal` — the fraction of runs producing at
least one repair equivalent to or stronger than an ideal.

| Sweep A (generations) | implies_ideal | | Sweep B (population) | implies_ideal |
|---|---|---|---|---|
| gen5 | 0.480 | | pop50 | 0.348 |
| gen10 *(current C–J point)* | 0.500 | | pop200 *(current C–J point)* | 0.500 |
| gen40 | 0.535 | | pop1000 | 0.517 |
| gen80 | 0.576 | | pop1500 | 0.527 |

Both curves are still climbing at their top level. Population matters most below
pop200 (0.348 → 0.500), then flattens; generations climb steadily throughout.

The C–J results at gen10/pop200 split into three groups.

**Real signal.** J (weakening) gives 0.575 off against 0.500 on. I (*mutation
rate*) moves 0.280 → 0.500 monotonically. C isolates one bad level (status-only
at 0.390) against four that are indistinguishable.

**Signal only at degenerate extremes.** D collapses to 0.007 at ptrig1.0 and F
collapses to 0.007 at ptim0.0. Both are structural: p_trigger=1.0 never mutates
the response, p_timing=0.0 never mutates the timing, so each locks out a
dimension the repair needs. Away from those cliffs D spans 0.460–0.517 and E
spans 0.460–0.502.

**No signal at all.** G (*model-count bound*) reads 0.492–0.505 across bound5 to
bound160 — a 32× parameter range producing a 1.3 percentage-point spread. H
(*crossover rate*) reads 0.495–0.512 across the full 0.0–1.0 range.

G and H are dropped from this campaign. That is the plan's most contestable
choice and is argued below.

## Cost model

Costs were measured directly on av3 rather than extrapolated. Sweeps A and B
cross at exactly one point (gen10/pop200), so any (generations, population) pair
off those axes is unmeasured, and a separable model fitted to A and B
overestimated gen40/pop1000 by ~30%. The calibration reproduces the campaign's
contention shape — four concurrent runs at `parallel = 8`, saturating 32 cores —
which is the shape the factorial ran under, confirmed from the `parallel = 8`
line in its per-run `config.toml` snapshots.

Seconds per seed, summed over all four specifications, `counter` + `compare`:

| Operating point | weakening on | weakening off | ratio |
|---|---|---|---|
| gen10/pop200 *(old C–J point)* | 15.6 | — | 1.64× *(from CSV)* |
| gen20/pop500 | 39.2 | 53.5 | 1.36× |
| gen30/pop500 | 54.3 | 75.5 | 1.39× |
| gen30/pop1000 | 71.6 | 87.9 | 1.23× |
| gen40/pop1000 | 88.9 | 112.3 | 1.26× |
| gen60/pop1000 | 120.6 | — | — |

Weakening-off costs **1.26×** at gen40/pop1000 — *cheaper*, proportionally, than
the 1.64× it cost at gen10/pop200. The penalty for disabling the filter shrinks
as the operating point grows.

But those numbers are **too low**, and the reason is a flaw in how they were
taken. Each ran four jobs on four workers, one per specification. The
specifications finish at wildly different times (takeoff 14 s, fsm-combined
50 s), so the short ones retire early and hand their cores to the long ones —
which then run faster than they ever would in the campaign, where the queue
holds thousands of runs and stays saturated. Re-measuring at 32 jobs on 4
workers, 8 deep:

| gen40/pop1000, sum over 4 specs | 4-job batch | saturated | error |
|---|---|---|---|
| weakening on | 88.9 s | **112.8 s** | +27% |
| weakening off | 112.3 s | **141.5 s** | +26% |

The saturated run measured a 3.79× speedup against an ideal of 4.0, so the queue
really is busy and the figures are not a scheduling artefact. Everything below
uses the saturated numbers. The 4-job figures survived a check against history
(gen10/pop200 came to 15.6 s against 15.2 s implied by the CSV) only because at
that operating point the specs differ by ~2 s rather than ~35 s, so there was
almost no tail to exploit — the check was real but did not generalise.

Re-measured on `587a5b6` after `4ba4c91` raised the model-counting bound to the
timing horizon, since that commit changes `semantic_similarity.cpp` and the whole
budget rests on its cost:

| gen40/pop1000, saturated | a37a911 | 587a5b6 | change |
|---|---|---|---|
| weakening on | 112.8 s | 112.7 s | −0.1% |
| weakening off | 141.5 s | 142.4 s | +0.6% |

Both within noise, so `4ba4c91`'s "raising k to the horizon is close to free"
holds and the budget is unchanged. Throughput 3.80× of an ideal 4.0; 0 failures
and 0 dropped individuals across 32 runs.

Wall-clock is `25.44 × seeds × (112.7 + 142.4) / 8`, where 8 is four concurrent
jobs on each of two machines:

| Seeds | Wall-clock |
|---|---|
| 90 | 20.3 h *(over budget)* |
| **80** | **18.0 h** |
| 70 | 15.8 h |

`compare` is not modelled separately. `wall_time_s` in the results CSV measures
only the `counter` subprocess, and a comment in `run_experiments.py` warns of a
141-second `compare` on a 29-repair fsm-timing run — but repair counts here top
out at 14, and `compare` measured 0.2–0.9 s across all four specs. It is ~2% of
a run and is included in the table above.

## The plan

| Parameter | Value |
|---|---|
| Sweeps | C, D, E, F, I |
| Crossed factor | `run_weakening` ∈ {on, off} |
| Operating point | generations=40, population_size=1000 |
| Selection | `nsga2` only |
| Specifications | takeoff, fsm, fsm-timing, fsm-combined |
| Seeds | planned 0–89; **stop at the 18 h deadline**, expect ~80 |
| Jobs per machine | 4, each with `parallel = 8` |
| Distinct runs | 54 × 4 specs × seed count |
| Result rows | 62 × 4 specs × seed count |
| Estimated wall-clock | 18 h for ~80 seeds, both machines in parallel |

Sweeps C, D, E, F and I define 31 levels. Within each weakening state the five
default levels (C/default, D/ptrig0.5, E/presp0.5, F/ptim0.15, I/mut1.0) are
byte-identical and collapse to one canonical run, giving 27 distinct runs per
state and 54 across both. Weighting each level by its measured cost relative to
the baseline gives 25.44 effective levels per state; the multipliers span 0.37×
(ptrig1.0) to 1.37× (ptrig0.0) but very nearly cancel, averaging 1.018×.

**Seeds are not fixed in advance, and that is deliberate.** The cost model has
already been wrong once by 27%, so committing to a seed count in advance means
either overshooting the budget or leaving it unspent. Instead `run_experiments.py`
now orders work **seed-major**: every level, specification and weakening state
completes for seed 0 before seed 1 starts. The campaign is launched over seeds
0–89 and killed at the deadline, and whatever has finished is a *balanced*
design at however many seeds it reached — every cell sampled equally, just fewer
seeds. The natural config-major order would instead have finished the first
levels and left the last ones at zero, which is not analysable at all. Both
machines run the same 27 levels over disjoint seed ranges, so each stops at a
whole number of seeds independently.

At 18 h the expected yield is ~80 seeds (0.225 h per seed per machine). The
`--seeds` split is av2 0–44, av3 45–89.

### Pros and cons of the choices

**Weakening as a crossed factor rather than a sweep.** This is the whole point
of the redesign: it makes the weakening main effect measurable at the new
operating point *and* makes its interaction with every other swept parameter
measurable, instead of pinning weakening at its default while the others vary.
It costs 2.3× the one-factor-at-a-time plan. The main effect is very well
powered — every one of the 27 levels contributes to both arms, so the on-vs-off
contrast pools ~9,720 runs per side. The interactions are not (see below).

**Dropping G and H.** This is the weakest link in the plan and the first thing
to revisit. G and H would have been 10 of 38 runs (~28% of the budget) to
re-test two parameters that showed a combined 2.6-point spread across their
entire ranges. Spending that on seeds instead takes the campaign from ~62 to ~90
seeds and tightens every interval. But flatness at pop200 is *not* evidence of
flatness at pop1000, and crossover is the parameter most likely to wake up at a
larger population, since crossover needs a diverse pool to have anything to
recombine. Dropping H specifically forfeits the weakening × crossover
interaction, which is the one interaction with a mechanistic story behind it:
both parameters govern population diversity. If a second campaign is affordable,
H is what it should contain.

**generations=40, population_size=1000.** Both parameters increase together,
which is what the A and B curves ask for — 4× and 5× the old setting. The design
stays one-factor-at-a-time in the other parameters, so C–I are measured at one
new operating point rather than across a grid; a parameter whose optimum *moves*
with population will not be traced. gen60/pop1000 was rejected at 16.2 h for the
narrower plan and does not fit at all once weakening doubles the arms. gen80 was
the best level measured in sweep A (0.576) and this plan does not reach it, so
C–I are still not read at the GA's best-known setting — just much closer to it.

**~80 seeds.** Fewer than the factorial's 100, so a level here is not *exactly*
seed-matched to the old data, though the seeds that do run are a prefix of the
same range and compare directly. At p≈0.5 the 95% confidence interval is ±5.5
percentage points per level pooled across specs, and ±11.0 points per spec. The
seed-major ordering means this number is a floor discovered at the deadline
rather than a promise made in advance.

**NSGA-II only.** Halves the cost, and the factorial already settled the scheme
comparison. Every conclusion is now conditional on NSGA-II, and the two schemes
rank levels differently in principle. Given weighted is not the intended
production scheme, that is acceptable.

**Keeping the degenerate levels (ptrig1.0, ptim0.0).** Both read 0.007 and both
will stay there at any operating point, since the cause is structural. They cost
little (ptrig1.0 runs at 0.37× baseline) and they anchor the low end of their
sweeps, making the non-degenerate levels legible as a curve rather than a flat
line. They also become interesting *because* of the crossing: whether weakening
rescues a degenerate mutation setting is a real question this design can answer.

### What this campaign cannot answer

Interactions are differences of differences, so their standard error is roughly
double a main effect's. At ~80 seeds × 4 specs a per-level interaction carries a
95% interval of about **±11 percentage points** — wider than the 7.5-point
weakening effect it would be trying to explain. Resolving a 7-point interaction
per level needs ~200 seeds, which does not fit at any operating point inside 18
hours.

So the honest deliverable is three-tiered. The **weakening main effect at
gen40/pop1000** is precise. **Sweep-level trends** — does weakening's benefit
grow or shrink across the mutation-rate range? — are readable, because pooling
five levels multiplies the effective sample. **Per-level interactions** are
underpowered and should be reported with intervals, not as findings. Any single
level that appears to flip is noise until a targeted follow-up says otherwise.

### The engine moved: no comparison with the July-14 factorial

`4ba4c91` (2026-07-15) takes the effective model-counting bound as
`min(max(cfg_bound, horizon), ceiling)` rather than the configured bound. Below
the timing horizon a bounded timing's safety automaton had not yet reached its
stuck transition, so it counted traces that had already missed the deadline —
breaking the documented invariant `conjunction_count <= min(individual counts)`
and pushing the harmonic mean of the containment ratios above 1. Semantic
similarity is one of the four fitness objectives, so this changes which repairs
win.

The campaign therefore runs on `587a5b6`, and **`results-factorial.csv` is not a
valid comparator for it** — any difference confounds operating point with engine
version ([[stale-results-after-engine-change]]). Two consequences:

- The "old vs new operating point" figure cannot be drawn from existing data. A
  same-engine gen10/pop200 arm (C–I × weakening) would cost ~2.9 h — about 13
  seeds of the main campaign — and is the only way to make that claim rigorously.
  It is *not* in the plan above; sweeps A and B already established that the
  operating point matters, so this is a nice-to-have.
- The pre-fix reading that G (bound) "shows no signal" is now suspect: `4ba4c91`
  overrides low configured bounds up to the horizon, so G's old levels were
  partly measuring the broken invariant rather than the bound. G is dropped from
  this campaign anyway, but the argument for dropping it is weaker than the flat
  0.492–0.505 numbers suggest, and it should not be cited as settled.

### Known defect: the SPOT tautology bug

`ltl2tgba` from SPOT 2.15.1 exits 2 on some formulae the GA generates, printing
`print_hoa(): automaton is complete but prop_complete()==false`. It is
deterministic per formula and is triggered by `-D` or `-S` independently, both
of which `spot.cpp` passes; `-H` alone is clean. Minimal repro:

```sh
build-release/third_party/spot/bin/ltl2tgba -D -S -H -f 'G((a & !((b & !c) -> d)) -> b)'
```

SPOT 2.14 does not reproduce it, so the bug arrived in 2.15. The decision is to
tolerate it: `193e3dd` contains a scoring failure to the individual behind a
circuit breaker (`Config::max_scoring_failure_rate`, default 0.05), so an
affected individual is dropped rather than aborting the run, and `n_dropped`
already records it in the results CSV. Pinning SPOT to 2.14 would remove the
drops but is an engine change, and would make this dataset non-comparable with
the July-14 factorial ([[stale-results-after-engine-change]]). The drop rate
will be measured on the current binary before launch and reported alongside the
results; if it correlates with the weakening arm, it biases exactly the contrast
this campaign exists to measure and must be reported as such.

### Correctness hazards

**Stale binaries.** `build-release/counter` on this workstation was built
2026-07-14 16:36, which predates `193e3dd` (19:23, scoring circuit breaker),
`4895024` (19:42, the `run_bounded_async` use-after-free fix) and `a37a911`
(2026-07-15 13:18, the `signal_tracer` process-leak fix). Runs against it
SIGSEGV at gen40/pop1000 and leak hung `signal_tracer` processes. All three
machines were rebuilt at `a37a911` before launch. A binary's mtime must be
checked against the fix commits, not its checkout's HEAD — the checkout was
already at `a37a911` while the binary was three commits behind.

**Shared output directories.** The campaign reuses the same
`(sweep, level_name, selection, spec, seed)` CSV keys and the same `run_id`
directory names as the factorial. Resume skips on CSV key and never cleans
output directories, so pointing this run at the existing `experiments/results/`
would make `parse_repair_files()` read stale gen10/pop200 `repair_*.json` and
silently report them as gen40/pop1000 results. The profile therefore gets its own
`results_dir`, `configs_dir` and `results_csv`, and `run_id` includes the
weakening state so the two arms cannot read each other's repairs.

**Timeouts are false zeros.** A timed-out run is recorded with
`implies_ideal = 0` and `timed_out = 1` rather than being omitted, so an
analysis that does not filter `timed_out` counts it as a failure to repair. The
caps are set at 600 s (900 s for fsm-combined) against a measured worst case of
~41 s — 15–20× margin — because a cap that bites costs one biased row, while a
slow run costs only time.

## Script changes

**`gen_configs.py`** gains `--generations`, `--population-size`, `--schemes`,
`--sweeps`, `--weakening` and `--out-dir`. Defaults reproduce today's output
byte-identically, verified by `diff -r` against the existing 126 config files.

```sh
python scripts/gen_configs.py --generations 40 --population-size 1000 \
    --schemes nsga2 --weakening both --sweeps C D E F I \
    --out-dir experiments/configs-cj-large
```

**`run_experiments.py`** gains per-profile `configs_dir`, `results_dir` and
`baseline_aliases`, a `weakening` CSV column modelled on the existing
`selection` column (with a `LEGACY_WEAKENING` default so resume still works
against older CSVs), and a `cj-large` profile. Weakening is a column rather than
part of `level_name` because `level_value_of()` parses a trailing number off the
level name and would misread it.

### Launch

```sh
# on av2
python scripts/run_experiments.py --profile cj-large --jobs 4 --seeds $(seq 0 44)
# on av3
python scripts/run_experiments.py --profile cj-large --jobs 4 --seeds $(seq 45 89)
```

Merge the two CSVs with `scripts/merge_experiments.py`. Rows are appended under
a lock, so an interrupted run resumes without losing completed rows.

## Reducing the analysis

`scripts/analyse.ipynb` stores **80 figures across 240 panels**, all produced by
one helper, `analyse_sweep()`, called ten times. It plots three panels
(`best_fitness` boxplot, `implies_ideal` rate, `found_repair` rate) for every
combination of 2 schemes × 4 specs × 10 sweeps. A wall-time cell exists but has
never been executed.

Most of that is redundant. `found_repair` is a weaker precondition of
`implies_ideal` — every run that implies an ideal necessarily found a repair —
so its panel adds nothing. `best_fitness` is the GA's internal objective, a
*proxy*; the notebook's own statistics show it moving 0.8153 vs 0.7969 where
`implies_ideal` moves six-fold. And faceting by spec *and* scheme multiplies each
sweep into eight figures where hue and linestyle would carry both.

The reduced notebook keeps four figures:

1. **Weakening on vs off, per sweep** — small multiples, `implies_ideal` on the
   y-axis, level on the x-axis, weakening as hue. This is the headline: the main
   effect and every interaction read off one grid.
2. **Old vs new operating point** — *only if a same-engine baseline is run*
   (see below). The July-14 factorial is no longer a valid comparator.
3. **Wall-clock cost by level and weakening state** — median seconds, log axis.
   Needed to read any improvement as a trade against cost, and currently never
   rendered.
4. **Cross-sweep summary table** — already present, no plot.

Selection scheme drops out of the faceting entirely, since the campaign is
NSGA-II only. `best_fitness` survives as Kruskal–Wallis/Dunn statistics in text,
without the 80 boxplot panels. That takes the notebook from 80 figures to 3 plus
a table.
