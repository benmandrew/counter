# counter against AuRUS at 1000 individuals: level pooled, split per family, and 54.7% of the machine time on kills

Four arms over 25 *Temporal Logic Synthesis Format* (TLSF) families at seeds 0 to 29, 3000 runs, 750 an arm. `genetic.selection_scheme` (`nsga2-apportion`, `weighted`) crosses `fitness.status_grading` (`mrs`, `aurus`), and everything else is shared: `termination = "individuals"`, `max_individuals = 1000`, `population_size = 100`, `parallel = 1`, a 500-generation ceiling, a 7200 s external cap, weights 0.1 / 0.2 / 0.7, the log similarity metric, the weakening screen off, `accumulate_repairs` on and monolithic repair mode. Those are AuRUS's own stopping rule and population, read off the settings banner of its 679 archived logs, so this is the first campaign in which both tools search the same number of individuals and wall time is an outcome rather than a budget.

Every figure below is the output of `python3 scripts/analyse_matched.py`, run from this directory over the merged *comma-separated values* (CSV) file `results-matched.csv`, the two per-host CSVs, the per-run manifests and `curves-matched.csv`. `PLAN.md` was written on 2026-08-29 before any row existed, and its section 2 registers no decision rule and no primary endpoint. Every p-value in this report is therefore *post-hoc* and is labelled so where it appears; a test chosen after the curves were drawn describes this sample and tests no hypothesis.

Provenance is `ec0abe0` on branch `campaign/aurus-matched`, held by the annotated tag `provenance/aurus-matched`, which was cut before the branch moved. All 3000 rows record `commit=ec0abe0, dirty=0`. av2 held seeds 0 to 14, starting 2026-08-29T13:45:04+0100 and finishing 2026-08-31T03:09:18+0100 over 36.15 h; av3 held seeds 15 to 29 over 37.03 h. The merge reads 3000 rows over 3000 distinct `(arm, spec, seed)` keys with no duplicates. The split is over seeds alone, so every contrast runs both of its arms on one host.

## Censoring, read before the endpoints

315 of the 3000 runs (10.5%) were killed at the 7200 s cap. A killed run writes no `run.json` and scores as a failure in every column of the results CSV, so it reads as no repair found and no ideal implied. The kills sit in seven families: `humanoid-742` on 120 of 120, `humanoid-531` on 93, `pcar-v2-888` on 44, `prioritized-arbiter-aurus` on 24, `humanoid-503` on 20, and `full-arbiter-aurus` and `lift` on 7 each. Per arm the *censored* share is 0.093 under `nsga2-apportion/mrs`, 0.052 under `nsga2-apportion/aurus`, 0.177 under `weighted/mrs` and 0.097 under `weighted/aurus`.

The kills cost more than they count. The 2685 runs that finished took 522.1 core-hours, 45.3% of the campaign, and the 315 that were killed took 630.0, 54.7% of it, for a total of 1152.1 core-hours. The plan's estimate of 500 s a run was low by 2.77x against a measured mean of 1382.5 s, and the campaign took 37.0 h against the 13 h estimated. Only the *accumulator*, which flushes every gate-passing candidate to `accumulated/index.tsv` as it is found, records what a killed run found.

A kill is not symmetric between the tools. AuRUS writes its solution files in one batch when a run ends, so a run killed at its cap loses all of them and carries no verdict; counter's accumulator keeps every candidate, and the results CSV still scores the run as zero. The two biases run opposite ways, and the head-to-head below is read both ways for that reason.

## The budget bound as declared

All 2685 manifests read `stopped_by = individuals` and none reads `generations`; `generations_run` ran 25 to 31 against the 500 ceiling. `individuals_bred` is exactly 1000 on every manifest, with zero overshoot where the between-slot check permits a small one. The join behind the curves agrees with the results CSV: `implies_ideal` is 1403 of 3000 (46.8%) there, and the curve's `ideal_solutions` exceeds zero on 1421 (47.4%). The ceiling never bound.

## Cost, by arm

| arm | yield | `implies_ideal` | censored | mean wall s | median wall s |
|---|---|---|---|---|---|
| `nsga2-apportion/mrs` | 0.907 | 0.484 | 0.093 | 1216.2 | 122.5 |
| `nsga2-apportion/aurus` | 0.896 | 0.484 | 0.052 | 827.0 | 93.9 |
| `weighted/mrs` | 0.823 | 0.460 | 0.177 | 2094.9 | 659.2 |
| `weighted/aurus` | 0.863 | 0.443 | 0.097 | 1392.0 | 416.8 |

Yield and `implies_ideal` are over all 750 runs an arm with a kill counting as a failure; wall time is over finished runs. The median sits far below the mean in every arm, 122.5 s against 1216.2 s under `nsga2-apportion/mrs`, because a few families carry the tail: `humanoid-742` at a mean of 7200.0 s, `humanoid-531` at 6702.9 s, `pcar-v2-888` at 4503.4 s, `humanoid-503` at 3375.4 s, `lift` at 2445.0 s and `prioritized-arbiter-aurus` at 2426.7 s. The search itself is 25 to 31 generations. The pre-flight run in `PLAN.md` section 8 spent 65% of its wall time after the search, in the gate and the implication filter, and nothing in this archive measures that split per run.

## Curves, by arm

Each curve is the mean count per run at a log-spaced cut, carrying a stopped run's last value forward, since what a run had found by time t is what it had when it stopped. Five of the thirteen cuts are shown.

| `solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| 10 | 23.48 | 19.93 | 44.80 | 33.29 |
| 50 | 59.91 | 46.64 | 209.17 | 160.32 |
| 200 | 75.28 | 53.80 | 257.61 | 187.84 |
| 1000 | 88.32 | 59.85 | 303.29 | 216.21 |
| 7200 | 94.27 | 61.81 | 340.68 | 231.62 |

| `ideal_solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| runs scored | 712 | 720 | 678 | 685 |
| undecided | 38 | 30 | 72 | 65 |
| 10 | 2.13 | 2.02 | 3.61 | 2.95 |
| 50 | 5.25 | 4.90 | 10.33 | 9.58 |
| 200 | 5.57 | 5.12 | 11.65 | 10.61 |
| 1000 | 5.60 | 5.18 | 11.71 | 10.63 |
| 7200 | 5.61 | 5.20 | 11.72 | 10.63 |

The weighted arms accumulate far more gate-passing candidates, 340.68 a run at 7200 s under `weighted/mrs` against 94.27 under `nsga2-apportion/mrs`, and more ideal-implying ones, 11.72 against 5.61. Both curves are flat past 200 s in every arm; `nsga2-apportion/mrs` reads 5.57 ideal solutions at 200 s and 5.61 at 7200 s. The ideal curve counts only the runs `compare` could score, and the `undecided` row is the runs it could not, which the maximality section explains.

Discovery is fast where it happens at all. `time_to_first_repair` is reached on 750 of 750 runs under both `mrs` arms, on 711 (94.8%) under `nsga2-apportion/aurus` and on 720 (96.0%) under `weighted/aurus`, at a median of 3.3 to 3.7 s among reachers. `time_to_first_ideal_repair` is reached on 367 of 712 (51.5%), 366 of 720 (50.8%), 355 of 678 (52.4%) and 333 of 685 (48.6%), at medians of 3.0 to 3.7 s. AuRUS reaches a first ideal solution on 410 of its 708 usable runs (57.9%) at a median of 9.0 s, 72 of its 780 runs having been lost at the cap with no verdict. Its resolution is one iteration dated to the second where counter's is sub-second, so the two medians are read at the coarser one.

## The 2x2, post-hoc

Both contrasts below are post-hoc. `PLAN.md` section 2 registers no rule, so they describe this sample and test nothing. Each pairs 1500 `(spec, seed)` cells across the factor's two levels, a kill scoring as a failure, and reads them with an exact two-sided *McNemar test*.

Selection separates. `nsga2-apportion` alone finds a repair on 122 pairs against `weighted` alone on 34 (p = 0.0000, post-hoc), implies an ideal on 119 against 70 (p = 0.0004, post-hoc), and is faster on 1333 of 1500 pairs, at a mean of 1021.6 s against 1743.4 s. This is a different reading from `2026-08-26-selection-smoke`, which had the same two schemes level on `implies_ideal` at p = 0.607591 and separated on cost alone; that campaign ran gen 10 / pop 200 at `jobs = 8`, and this one runs 1000 individuals at population 100 single-threaded, so the two are not one measurement. Part of the separation is censoring, the weighted arm's kill rate of 0.177 under `mrs` against 0.093 counting as a failure on both endpoints.

Status grading does not separate. `mrs` alone finds a repair on 61 pairs against `aurus` alone on 83 (p = 0.0798, post-hoc), implies an ideal on 91 against 78 (p = 0.3560, post-hoc), and is faster on only 441 of 1500 pairs, at a mean of 1655.5 s against 1109.5 s. At this budget the `mrs` grading buys nothing this sample can read and costs wall time.

## Head to head against AuRUS

AuRUS's per-family rates come from its archived logs on av2 through the two vendored `aurus` scripts. They are unfiltered by the well-separation screen the 2026-08-14 campaign applied, that screen's inputs having survived on neither host. `all` counts a killed run as a failure on both sides, `sc` drops killed runs on both sides, and `k` is the number killed.

| family | counter all | counter sc | k | AuRUS all | AuRUS sc | k | d-all | d-sc |
|---|---|---|---|---|---|---|---|---|
| `arbiter-aurus` | 0.592 | 0.592 | 0 | 0.933 | 0.933 | 0 | -0.342 | -0.342 |
| `detector-aurus` | 0.950 | 0.950 | 0 | 1.000 | 1.000 | 0 | -0.050 | -0.050 |
| `full-arbiter-aurus` | 0.492 | 0.522 | 7 | 0.000 | 0.000 | 9 | +0.492 | +0.522 |
| `gyro-var1` | 0.008 | 0.008 | 0 | 0.067 | 0.067 | 0 | -0.058 | -0.058 |
| `gyro-var2` | 0.108 | 0.108 | 0 | 0.700 | 0.700 | 0 | -0.592 | -0.592 |
| `humanoid-458` | 0.000 | 0.000 | 0 | 0.000 | 0.000 | 0 | +0.000 | +0.000 |
| `humanoid-503` | 0.025 | 0.030 | 20 | 0.000 | -- | 30 | +0.025 | -- |
| `humanoid-531` | 0.058 | 0.259 | 93 | 0.000 | 0.000 | 7 | +0.058 | +0.259 |
| `humanoid-742` | 0.000 | -- | 120 | 0.933 | 0.933 | 0 | -0.933 | -- |
| `lift` | 0.000 | 0.000 | 7 | 0.367 | 0.367 | 0 | -0.367 | -0.367 |
| `lily02` | 1.000 | 1.000 | 0 | 0.867 | 0.897 | 1 | +0.133 | +0.103 |
| `lily11` | 0.675 | 0.675 | 0 | 0.900 | 0.900 | 0 | -0.225 | -0.225 |
| `lily15` | 0.050 | 0.050 | 0 | 0.133 | 0.133 | 0 | -0.083 | -0.083 |
| `lily16` | 0.017 | 0.017 | 0 | 0.000 | 0.000 | 0 | +0.017 | +0.017 |
| `load-balancer-aurus` | 0.842 | 0.842 | 0 | 0.900 | 0.900 | 0 | -0.058 | -0.058 |
| `ltl2dba-r-2` | 1.000 | 1.000 | 0 | 1.000 | 1.000 | 0 | +0.000 | +0.000 |
| `ltl2dba-theta-2` | 1.000 | 1.000 | 0 | 0.967 | 0.967 | 0 | +0.033 | +0.033 |
| `ltl2dba27` | 1.000 | 1.000 | 0 | 0.933 | 0.933 | 0 | +0.067 | +0.067 |
| `minepump` | 0.533 | 0.533 | 0 | 1.000 | 1.000 | 0 | -0.467 | -0.467 |
| `pcar-v2-888` | 0.033 | 0.053 | 44 | 0.000 | 0.000 | 1 | +0.033 | +0.053 |
| `prioritized-arbiter-aurus` | 0.358 | 0.448 | 24 | 0.000 | 0.000 | 24 | +0.358 | +0.448 |
| `rg1` | 0.025 | 0.025 | 0 | 0.000 | 0.000 | 0 | +0.025 | +0.025 |
| `rg2` | 0.983 | 0.983 | 0 | 1.000 | 1.000 | 0 | -0.017 | -0.017 |
| `round-robin-arbiter-aurus` | 0.958 | 0.958 | 0 | 1.000 | 1.000 | 0 | -0.042 | -0.042 |
| `simple-arbiter-aurus` | 0.983 | 0.983 | 0 | 0.967 | 0.967 | 0 | +0.017 | +0.017 |

Pooled, the contrast is null both ways, and both reads are post-hoc. Over all 25 families with a kill as a failure, counter is higher on 11, lower on 12 and tied on 2, at a mean difference of -0.079 and an exact two-sided *Wilcoxon signed-rank test* p = 0.2896. Over the 23 families with scorable runs on both sides, counter is higher on 10, lower on 11 and tied on 2, at -0.033 and p = 0.5392. The 2026-08-21 ship campaign read the same contrast at counter's own budget as 0.502 against 0.504, p = 0.7549; this campaign reads it at AuRUS's budget, which was the reason for running it.

The pooled null hides a per-family split. counter leads on the arbiter families AuRUS never repairs, `full-arbiter-aurus` at 0.492 against 0.000 and `prioritized-arbiter-aurus` at 0.358 against 0.000, and on `lily02` at 1.000 against 0.867. AuRUS leads on `gyro-var2` at 0.700 against 0.108, `minepump` at 1.000 against 0.533, `lift` at 0.367 against 0.000, `arbiter-aurus` at 0.933 against 0.592 and `lily11` at 0.900 against 0.675. The other families sit within 0.09 of each other on the all-runs read.

`humanoid-742` is the row to read with care. Every one of counter's 120 runs there was killed at the 7200 s cap, so the results CSV scores all of them as failures and the head-to-head reads 0.000 against AuRUS's 0.933; 118 of the 120 have no `compare` verdict at all, and all 120 maximal curves are partial. Its curves exist only because the accumulator flushes as it goes. Nothing in this archive can say what counter finds on `humanoid-742`, and the 0.000 is the cap rather than the search. `humanoid-503` is the mirror image: AuRUS was killed on 30 of 30 runs there and carries no verdict, so the family drops out of the scorable read, while counter's 20 kills of 120 leave it at 0.025.

## Maximality, with two caveats

`maximal_solutions` and `maximal_ideal_solutions` were scored offline on av2 and av3 by `scripts/score_curves.py --maximality --cuts 20 --jobs 4 --maximal-timeout 900 --deadline-s 4500`, one invocation a run, eight workers a host on a smallest-first queue. All 3000 runs have a curve file and none failed. Two `maximal` binaries produced them. 1956 curves come from the build before 2026-09-03, which ran `ltlfilt --simplify` on every whole-spec implication query, and 1044 from `d4f8be4`, which skips that pass and gives SPOT black's budget, after the pass was measured at 95.6% of solver wall time on these queries. The two agree on every one of 264 cut-values over a 10-run sample. The switch retired 256 partial curves the old build had produced and re-scored them, and 716 worker-hours were spent after the switch alone.

| `maximal_solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| runs scored | 750 | 711 | 750 | 720 |
| 10 | 9.10 (0) | 8.55 (0) | 12.12 (0) | 10.10 (0) |
| 50 | 19.92 (1) | 16.45 (0) | 40.44 (2) | 31.55 (0) |
| 200 | 25.55 (15) | 19.14 (2) | 53.89 (35) | 39.31 (13) |
| 1000 | 30.01 (41) | 21.73 (24) | 61.81 (97) | 44.65 (66) |
| 7200 | 30.86 (44) | 22.01 (31) | 65.30 (135) | 45.12 (70) |

| `maximal_ideal_solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| runs scored | 712 | 681 | 678 | 655 |
| undecided | 38 | 30 | 72 | 65 |
| 10 | 0.96 (0) | 0.98 (0) | 1.14 (0) | 1.05 (0) |
| 50 | 2.06 (0) | 2.05 (0) | 2.29 (2) | 2.13 (0) |
| 200 | 2.22 (2) | 2.13 (0) | 2.51 (17) | 2.20 (1) |
| 1000 | 2.26 (7) | 2.17 (1) | 2.56 (33) | 2.22 (4) |
| 7200 | 2.28 (10) | 2.17 (3) | 2.69 (63) | 2.22 (6) |

The first caveat is the partial curves. 280 of the 3000 maximal curves (9.3%) are partial: a cut that exceeded the 900 s per-cut budget was abandoned and the curve ends there, reaching between 2 and 17 of 20 cuts, median 6. By family they are `humanoid-742` on all 120, `humanoid-531` 64, `pcar-v2-888` 31, `humanoid-503` 26, `prioritized-arbiter-aurus` 24, `lift` 9, `round-robin-arbiter-aurus` 4 and `full-arbiter-aurus` 2. The analysis reads a partial curve as undecided past its last computed cut rather than carrying its last value forward, and the bracketed `(n)` beside each cell is the number of runs so excluded at that cut. The weighted arms lose the most, 135 of 750 under `weighted/mrs` at 7200 s, so their late-cut means are over a set thinned of the runs that accumulated most.

The second is the undecided runs. 205 runs have no ideal metric at all: `compare`, which joins each candidate to the family's ideals, exceeded the harness's 600 s cap on them, so `ideal_solutions` and `maximal_ideal_solutions` are blank at every cut and the analysis reports them as `undecided`. By family they are `humanoid-742` on 118 of 120, `humanoid-531` on 68 of 120, `pcar-v2-888` 12, `lift` 4, `humanoid-503` 2 and `prioritized-arbiter-aurus` 1. The cause is the same simplify pass. The fix for `compare` is commit `2defe77` on this branch, verified over 26 runs to change no decided relation, and the campaign was deliberately not re-scored with it, so those 205 stand as timeouts. The old `compare` also left individual pairs undecided inside runs it did finish, one `humanoid-531` run reading `timeout` on 79 of its 95 candidate-ideal pairs, and an undecided pair counts as not ideal-implying. Every ideal count here is a lower bound. In the 26-run check none of those pairs became ideal-implying once decided.

Within those bounds, most of the weighted arms' surplus does not survive the *maximal antichain*. `weighted/mrs` holds 65.30 maximal solutions a run at 7200 s against 30.86 under `nsga2-apportion/mrs`, but the ideal-implying survivors read 2.69 against 2.28, and the four arms sit between 2.10 and 2.69 on that metric at every cut from 100 s on. The 340.68 gate-passing candidates a run the weighted arm accumulates, against 94.27 under `nsga2-apportion/mrs`, leave the two arms 2.69 against 2.28 on the metric that counts, on a set thinned of 63 runs against 10.

## Against the pre-registration

There was no rule to fire, and `PROVENANCE.json` records that in the field the other archives use for the decision taken. What the plan did commit to was the currency, and it held: 1000 individuals on every finished run, the ceiling never binding, and the 7200 s cap matching AuRUS's `GA_EXECUTION_TIMEOUT`. Section 7 named `humanoid-531` as the family to expect at the cap, and it was killed on 93 of 120. Section 13's cost estimate was low by 2.77x, which section 12 check 4 anticipated by requiring the cost be read off the campaign's own rows. Nothing here supports a claim against the paper's Table 2, the budget match being a code-level match on the weights, the six-level status ladder and the counting of an individual, as section 9 recorded before the run.

## Limitations

- **AuRUS's rates are unfiltered.** The 2026-08-14 campaign screened `implies_genuine` through well-separation from `validation-av2.csv` and `validation-av3.csv`, and neither file survives on either host, so the hit column is used as-is. The archived record of that screen is that it moved 2 of 25 family rates, `lift` 0.167 to 0.300 and `lily11` 0.900 to 0.833, a small correction in an unknown direction.
- **A kill is not symmetric.** counter's kills score as zero and AuRUS's drop out. The head-to-head is read both ways and headlines neither.
- **A solution means two things.** counter's curve counts gate-passing candidates flushed with sub-second timestamps; AuRUS's counts the running length of `ga.solutions` off its per-iteration log line, dated to the second and moving only at a generation boundary. The 415.62 solutions a run AuRUS reaches at 7200 s over its 692 shared-family runs, against counter's 61.81 to 340.68 per arm, are counts of different things.
- **Every ideal count is a lower bound.** 205 runs are undecided outright, and finished runs carry undecided pairs inside them.
- **280 maximal curves are partial**, concentrated in the families that accumulate most, and the late cuts of the weighted arms are read over the thinned set.
- **One weight setting and one population.** All four arms run 0.1 / 0.2 / 0.7 at population 100 single-threaded, so the selection contrast here and the selection-smoke null are at different populations and thread counts and neither reproduces the other.
- **Every p-value is post-hoc.** The plan registered no test, no alpha and no control arm.

## What is owed

A pairwise-relation flag on `compare`. The curves here are scored by carrying each cut's survivors into the next, 60.1M pairwise implication comparisons against 272.7M for re-running `maximal` over each whole prefix, over this campaign's 546,282 accumulated candidates. The textbook incremental antichain is about 12x: keep one running antichain, test each arrival against it alone, and discard it if anything dominates it. That needs an oracle answering whether one `.tlsf` file implies another, which `compare` computes over the full repairs x ideals cross (`src/compare.cpp:269-293`) and then collapses to the best relation per repair, hiding the domination the algorithm needs. It was not done here because the deployed 4.5x already took the pass from a day to a few hours, the remaining 2.7x is worth about four hours, and a rebuild would put a third source commit into `PROVENANCE.json` for an offline scoring binary.

`humanoid-742` is owed a run that can finish. 120 kills, 118 missing verdicts and 120 partial curves leave the family unmeasured on every metric, and the 0.000 in the head-to-head table will be read as a result unless the archive says otherwise. Re-scoring the 205 undecided runs under `2defe77` was available and deliberately not done; those 205 stand as timeouts, and a later reader who wants them decided has the commit.

Matching the budget removed the asymmetry the previous head-to-head carried and put a different one in its place, since the cap now bounds counter on families where AuRUS's kills carry no verdict at all. The figure that survives both readings is the pooled null, and the per-family split under it is what the next campaign has to be designed around.
