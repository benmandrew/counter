# counter against AuRUS at 1000 individuals, again: parity on the registered cell at p = 0.7216, the kill rate 10.5% to 6.4%, and humanoid-742 still at the cap

The same four arms as `2026-08-29-aurus-matched`, run again at the engine `main` carries after PR #171. 25 *Temporal Logic Synthesis Format* (TLSF) families at seeds 0 to 29, 3000 runs, 750 an arm. `genetic.selection_scheme` (`nsga2-apportion`, `weighted`) crosses `fitness.status_grading` (`mrs`, `aurus`), and everything else is shared: `termination = "individuals"`, `max_individuals = 1000`, `population_size = 100`, `parallel = 1`, a 500-generation ceiling, a 7200 s external cap, weights 0.1 / 0.2 / 0.7, the log similarity metric, the weakening screen off, `accumulate_repairs` on and monolithic repair mode. The generator line is the matched campaign's with the output directory changed. Four commits separate the two binaries on the search path: the equivalence collapse in the implication filter (`4ad290d`), the final-filter simplify skip (`1330f13`), and the `maximal` and `compare` simplify skips (`d4f8be4`, `11305f9`). The first campaign's result sat under a 10.5% kill rate that the final-filter speedup was expected to reduce, and it did, to 6.4%.

This campaign registers a rule where the first did not. `PLAN.md` was written on 2026-09-04 before any row existed; section 4 names the endpoint as per-family `implies_ideal` for `nsga2-apportion/mrs` against AuRUS over the 25 families, read with an exact two-sided *Wilcoxon signed-rank test* at alpha 0.05 with a killed run counting as a failure on both sides, and section 5 names three outcomes. Every figure below is the output of `python3 scripts/analyse_matched.py --results results-rematch.csv --curves curves-rematch.csv --runs-dir results-rematch --primary nsga2-apportion/mrs` at `d29cfc4`, run from this directory over the merged *comma-separated values* (CSV) file, the per-run manifests and `curves-rematch.csv`. The 2x2 contrasts and the pooled head-to-head are secondary by registration, and their p-values are labelled so where they appear; the analysis printout's closing footer still calls every p-value post-hoc, being the matched campaign's text, and the plan overrides it.

Provenance is `f8fbe26` on branch `campaign/aurus-rematch`, held by the annotated tag `provenance/aurus-rematch`, cut on 2026-09-07 before the close commits were written. `f8fbe26` branches from `main` at `34ebeaf` and adds the campaign declaration alone, so the search binary is `main`'s at that merge. All 3000 rows record `commit=f8fbe26, dirty=0`. av2 held seeds 0 to 14, starting the main phase at 2026-09-04T21:40:01+0100 and writing its last run directory at 2026-09-05T22:27:16+0100 over 24.8 h; av3 held seeds 15 to 29 over 25.1 h, against 36.15 h and 37.03 h in the archive. The merge reads 3000 rows over 3000 distinct `(arm, spec, seed)` keys with no duplicates. A 48-run calibration phase ran first on both hosts; its rows sit in `results-rematch-calib.csv` and enter no figure here.

## Censoring, read before the endpoints

192 of the 3000 run directories (6.4%) hold `accumulated/index.tsv` and no `run.json`, and 193 rows read `timed_out`; the one row beyond the directories is a `humanoid-503` `weighted/aurus` run that left no accumulated directory at all. The archive read 315 (10.5%). A killed run scores as a failure in every column of the results CSV, so it reads as no repair found and no ideal implied. By family the kills are `humanoid-742` on 107 of 120 against 120 in the archive, `humanoid-531` 56 against 93, `pcar-v2-888` 10 against 44, `prioritized-arbiter-aurus` 10 against 24, `humanoid-503` 8 against 20 and `lift` 1 against 7; `full-arbiter-aurus` lost 7 there and none here. Per arm the *censored* share is 0.040 under `nsga2-apportion/mrs`, 0.023 under `nsga2-apportion/aurus`, 0.117 under `weighted/mrs` and 0.077 under `weighted/aurus`, against 0.093, 0.052, 0.177 and 0.097.

The kills still cost more than they count. The 2807 runs that finished took 400.8 core-hours, 50.9% of the campaign, and the 193 that were killed took 386.0, 49.1% of it, for a total of 786.8 core-hours against the archive's 1152.1, with the archive's kill share at 54.7%. The mean run was 944.2 s against 1382.5 s and the median 156 s against 296 s, 0.68x the archive on the same design. `PLAN.md` section 10 carried the archive's figure as an upper bound and it held. Section 7 named the question the calibration turns on, whether `humanoid-742` and `humanoid-531` still cap, and the answer is split: the speedup reached `humanoid-531` and `pcar-v2-888` and left `humanoid-742` at 107 of 120.

A kill is not symmetric between the tools, and the AuRUS side is the archive's. AuRUS writes its solution files in one batch when a run ends, so a run killed at its cap loses all of them and carries no verdict; counter's *accumulator* flushes every gate-passing candidate to `accumulated/index.tsv` as it is found, so a killed counter run keeps its candidates and its curves while the results CSV scores it as zero. The two biases run opposite ways, and section 4 registers the kill-as-failure read as primary and the scorable read as secondary for that reason.

## The budget bound as declared

All 2807 manifests read `stopped_by = individuals` and none reads `generations`. `individuals_bred` is exactly 1000 on every manifest, with zero overshoot where the between-slot check permits a small one. The join behind the curves agrees with the results CSV: `implies_ideal` is 1431 of 3000 (47.7%) there against 1403 (46.8%) in the archive, and the curve's `ideal_solutions` exceeds zero on 1527 of 2999 runs (50.9%), the excess being killed runs that have curves and no verdict. The ceiling never bound.

## Cost, by arm

Each cell carries the archive's value in brackets. Yield and `implies_ideal` are over all 750 runs an arm with a kill counting as a failure; wall time is over finished runs.

| arm | yield | `implies_ideal` | censored | mean wall s | median wall s |
|---|---|---|---|---|---|
| `nsga2-apportion/mrs` | 0.960 (0.907) | 0.493 (0.484) | 0.040 (0.093) | 699.1 (1216.2) | 74.7 (122.5) |
| `nsga2-apportion/aurus` | 0.925 (0.896) | 0.503 (0.484) | 0.023 (0.052) | 607.5 (827.0) | 70.2 (93.9) |
| `weighted/mrs` | 0.883 (0.823) | 0.468 (0.460) | 0.117 (0.177) | 1435.2 (2094.9) | 279.5 (659.2) |
| `weighted/aurus` | 0.885 (0.863) | 0.444 (0.443) | 0.077 (0.097) | 1035.0 (1392.0) | 219.7 (416.8) |

Yield rose in every arm and `implies_ideal` moved by at most 0.019, so the runs the speedup rescued from the cap found repairs and rarely the ideal. The median sits far below the mean in every arm, 74.7 s against 699.1 s under `nsga2-apportion/mrs`, because the same families carry the tail: `humanoid-742` at a mean of 7081.7 s against 7200.0 s in the archive, `humanoid-531` at 4433.0 s against 6702.9 s, `pcar-v2-888` at 3240.2 s against 4503.4 s, `humanoid-503` at 2823.9 s against 3375.4 s, `prioritized-arbiter-aurus` at 1538.1 s against 2426.7 s and `lift` at 855.2 s against 2445.0 s. The final-filter skip moved every family in that list but the first.

## Curves, by arm

Curve values here and in the archive are not put in one table. `4ad290d` collapses mutually equivalent specifications in the implication filter, which shrinks the candidate set the maximality pass sees, and the pass ran under different `compare` and deadline budgets from the archive's; `PLAN.md` section 8 item 4 registered that before the run, and it holds whatever two tables look like side by side. Each curve is the mean count per run at a log-spaced cut, carrying a stopped run's last value forward, since what a run had found by time t is what it had when it stopped. Five of the thirteen cuts are shown.

| `solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| 10 | 23.34 | 18.51 | 44.62 | 29.89 |
| 50 | 59.76 | 46.30 | 208.48 | 158.81 |
| 200 | 75.01 | 53.43 | 256.70 | 187.01 |
| 1000 | 87.83 | 59.51 | 301.40 | 214.46 |
| 7200 | 94.28 | 61.64 | 340.60 | 231.88 |

| `ideal_solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| runs scored | 750 | 750 | 749 | 749 |
| undecided | 0 | 0 | 1 | 0 |
| 10 | 1.99 | 1.85 | 3.18 | 2.58 |
| 50 | 4.97 | 4.68 | 9.31 | 8.70 |
| 200 | 5.32 | 4.92 | 10.56 | 9.72 |
| 1000 | 5.45 | 5.03 | 10.70 | 9.83 |
| 7200 | 5.64 | 5.13 | 11.22 | 10.15 |

The weighted arms accumulate far more gate-passing candidates, 340.60 a run at 7200 s under `weighted/mrs` against 94.28 under `nsga2-apportion/mrs`, and more ideal-implying ones, 11.22 against 5.64. Both curves are flat past 200 s in every arm; `nsga2-apportion/mrs` reads 5.32 ideal solutions at 200 s and 5.64 at 7200 s. The `undecided` row is one run this time, against 30 to 72 an arm in the archive, which the maximality section explains.

Discovery is fast where it happens at all. `time_to_first_repair` is reached on 750 of 750 runs under both `mrs` arms, on 711 (94.8%) under `nsga2-apportion/aurus` and on 721 (96.1%) under `weighted/aurus`, at a median of 3.7 to 3.8 s among reachers. `time_to_first_ideal_repair` is reached on 399 of 750 (53.2%), 393 of 750 (52.4%), 379 of 749 (50.6%) and 356 of 750 (47.5%), at medians of 3.6 to 4.1 s. AuRUS reaches a first ideal solution on 410 of its 708 usable runs (57.9%) at a median of 9.0 s, 72 of its 780 runs having been lost at the cap with no verdict; those are the archive's rows, the arm not having been re-run. Its resolution is one iteration dated to the second where counter's is sub-second, so the two medians are read at the coarser one.

## The 2x2, registered secondary

`PLAN.md` section 5 makes both contrasts secondary and gives them no rule, so they describe this sample. Each pairs 1500 `(spec, seed)` cells across the factor's two levels, a kill scoring as a failure, and reads them with an exact two-sided *McNemar test*.

Selection separates again. `nsga2-apportion` alone finds a repair on 116 pairs against `weighted` alone on 28 (p < 0.0001, secondary), against 122 and 34 in the archive; implies an ideal on 135 against 72 (p < 0.0001, secondary), against 119 and 70; and is faster on 1366 of 1500 pairs, at a mean of 653.3 s against 1235.1 s, against 1333 of 1500 at 1021.6 s and 1743.4 s. Section 6 registered the hazard that the archive's selection contrast was partly a kill artefact, the weighted arms having been killed most, and named the shrinking of that contrast under compressed kill rates as the test. The kill rates did compress, 0.117 against 0.040 on the `mrs` arms where the archive read 0.177 against 0.093, and the contrast on `implies_ideal` widened. The selection contrast is not the kill rate.

Status grading separates on yield alone, and the direction moved. `mrs` alone finds a repair on 62 pairs against `aurus` alone on 38 (p = 0.0210, secondary), where the archive read 61 against 83 at p = 0.0798 with `aurus` ahead; it implies an ideal on 95 against 84 (p = 0.4549, secondary), against 91 and 78 at p = 0.3560; and it is faster on 629 of 1500 pairs, at a mean of 1067.2 s against 821.3 s, against 441 of 1500 at 1655.5 s and 1109.5 s. On the endpoint that matters the grading contrast is null a second time, and `mrs` still costs wall time.

## The registered primary against AuRUS

AuRUS's per-family rates are the archive's, from its logs on av2 through the two vendored `aurus` scripts, and unfiltered by the well-separation screen. The table is counter's registered cell alone, 30 runs a family. `all` counts a killed run as a failure on both sides, `sc` drops killed runs on both sides, and `k` is the number killed.

| family | counter all | counter sc | k | AuRUS all | AuRUS sc | k | d-all | d-sc |
|---|---|---|---|---|---|---|---|---|
| `arbiter-aurus` | 0.700 | 0.700 | 0 | 0.933 | 0.933 | 0 | -0.233 | -0.233 |
| `detector-aurus` | 1.000 | 1.000 | 0 | 1.000 | 1.000 | 0 | +0.000 | +0.000 |
| `full-arbiter-aurus` | 0.567 | 0.567 | 0 | 0.000 | 0.000 | 9 | +0.567 | +0.567 |
| `gyro-var1` | 0.000 | 0.000 | 0 | 0.067 | 0.067 | 0 | -0.067 | -0.067 |
| `gyro-var2` | 0.067 | 0.067 | 0 | 0.700 | 0.700 | 0 | -0.633 | -0.633 |
| `humanoid-458` | 0.000 | 0.000 | 0 | 0.000 | 0.000 | 0 | +0.000 | +0.000 |
| `humanoid-503` | 0.033 | 0.033 | 0 | 0.000 | -- | 30 | +0.033 | -- |
| `humanoid-531` | 0.200 | 0.200 | 0 | 0.000 | 0.000 | 7 | +0.200 | +0.200 |
| `humanoid-742` | 0.000 | -- | 30 | 0.933 | 0.933 | 0 | -0.933 | -- |
| `lift` | 0.000 | 0.000 | 0 | 0.367 | 0.367 | 0 | -0.367 | -0.367 |
| `lily02` | 1.000 | 1.000 | 0 | 0.867 | 0.897 | 1 | +0.133 | +0.103 |
| `lily11` | 0.367 | 0.367 | 0 | 0.900 | 0.900 | 0 | -0.533 | -0.533 |
| `lily15` | 0.000 | 0.000 | 0 | 0.133 | 0.133 | 0 | -0.133 | -0.133 |
| `lily16` | 0.033 | 0.033 | 0 | 0.000 | 0.000 | 0 | +0.033 | +0.033 |
| `load-balancer-aurus` | 1.000 | 1.000 | 0 | 0.900 | 0.900 | 0 | +0.100 | +0.100 |
| `ltl2dba-r-2` | 1.000 | 1.000 | 0 | 1.000 | 1.000 | 0 | +0.000 | +0.000 |
| `ltl2dba-theta-2` | 1.000 | 1.000 | 0 | 0.967 | 0.967 | 0 | +0.033 | +0.033 |
| `ltl2dba27` | 1.000 | 1.000 | 0 | 0.933 | 0.933 | 0 | +0.067 | +0.067 |
| `minepump` | 0.800 | 0.800 | 0 | 1.000 | 1.000 | 0 | -0.200 | -0.200 |
| `pcar-v2-888` | 0.100 | 0.100 | 0 | 0.000 | 0.000 | 1 | +0.100 | +0.100 |
| `prioritized-arbiter-aurus` | 0.433 | 0.433 | 0 | 0.000 | 0.000 | 24 | +0.433 | +0.433 |
| `rg1` | 0.033 | 0.033 | 0 | 0.000 | 0.000 | 0 | +0.033 | +0.033 |
| `rg2` | 1.000 | 1.000 | 0 | 1.000 | 1.000 | 0 | +0.000 | +0.000 |
| `round-robin-arbiter-aurus` | 1.000 | 1.000 | 0 | 1.000 | 1.000 | 0 | +0.000 | +0.000 |
| `simple-arbiter-aurus` | 1.000 | 1.000 | 0 | 0.967 | 0.967 | 0 | +0.033 | +0.033 |

Outcome 3 fired. Over the 25 families with a kill as a failure, counter's registered cell is higher on 12, lower on 8 and tied on 5, at a mean difference of -0.053 and an exact two-sided Wilcoxon p = 0.7216, so neither outcome 1 nor outcome 2 fires and the result is reported as parity with the discordant counts beside it. The registered secondary over scorable runs reads 23 families, `humanoid-742` and `humanoid-503` dropping out because one side of each has no scorable run, with counter higher on 11, lower on 7 and tied on 5, at -0.020 and p = 0.9238. The mean difference is negative while the sign count favours counter because the families counter loses it loses by more, `humanoid-742` at -0.933, `gyro-var2` at -0.633 and `lily11` at -0.533 against the largest gain of +0.567 on `full-arbiter-aurus`.

The pooled head-to-head over the four cells is secondary and reads the same way. Over all 25 families with a kill as a failure, counter is higher on 11, lower on 12 and tied on 2, at a mean difference of -0.070 and p = 0.3408, against 11, 12, 2, -0.079 and p = 0.2896 in the archive. Over the 24 families with scorable runs on both sides it is higher on 11, lower on 11 and tied on 2, at -0.032 and p = 0.6497, against -0.033 and p = 0.5392 over 23. The 2026-08-21 ship campaign read the contrast at counter's own budget as 0.502 against 0.504, p = 0.7549. This is the third null on the contrast and the first with a registered cell and rule.

The largest per-family differences are the kill asymmetry, on both sides. counter leads on `full-arbiter-aurus` at +0.567 and `prioritized-arbiter-aurus` at +0.433, where AuRUS scores 0 of 30 after being killed on 9 and 24 of those 30 with no verdict, and on `humanoid-531` at +0.200, where AuRUS was killed on 7 and the registered cell on none. AuRUS leads on `humanoid-742` at -0.933, where every one of counter's 30 registered-cell runs was killed, so the 0.000 is the cap rather than the search; the one cell that finished there is `nsga2-apportion/aurus`, at 13 of 30 against 0 of 30 in the archive, and in the pooled table `humanoid-742` reads 1.000 over those 13 scorable runs. `gyro-var2`, `lily11` and `lift` are the differences that carry no kill on either side: -0.633, -0.533 and -0.367 with `k = 0` throughout, so those three are search differences and stand as measured. `humanoid-503` is the mirror of `humanoid-742`, AuRUS having been killed on 30 of 30 there.

## Maximality, with caveats

`maximal_solutions` and `maximal_ideal_solutions` were scored offline on av2 and av3 by `scripts/score_curves.py --maximality --cuts 20 --jobs 4 --maximal-timeout 900`, one invocation a run under an outer timeout of the deadline plus 900 s, eight workers a host on a smallest-first queue with each worker pinned to four cores. One binary produced every curve, as `PLAN.md` section 8 item 1 requires: `maximal` and `compare` were both built from `f8fbe26` on both hosts and read back through `--version` before the pass, so no curve here was scored under the `ltlfilt --simplify` pass the archive's 1956 old-build curves were. All 3000 runs have a curve file and none failed. The pass ran from 2026-09-07T12:12 to 2026-09-08T08:17 on av2 and 08:15 on av3, 20.1 h of wall clock a host, for 154.3 and 157.3 worker-hours, 311.7 in all against the archive's 716.2 after its restart, 0.44x on the same design with `compare` and deadline budgets several times larger.

The budgets moved once. The pass launched at the plan's 900/600/4500 (maximal/compare/deadline) and was raised to 900/3000/7500 about an hour in, before the queue reached any run of the affected families, after the calibration pass showed `compare` exceeding 600 s on `humanoid-742` weighted runs and two unbounded `compare` calls on calibration directories of that family measured 1590 s over 258 relations and 1324 s over 319. The 647 runs scored under the first budgets are the smallest in the queue, 327 on av2 and 320 on av3, costing 14.0 worker-hours between them; every run of `humanoid-742`, `humanoid-531`, `pcar-v2-888`, `humanoid-503` and `prioritized-arbiter-aurus` ran under the second. Each run's timings line records the three budgets it ran under.

The calibration priced the pass before the 2952 other runs were committed, as section 8 item 2 required. 46 of the 48 calibration directories were scored at 900/600/4500 on 2026-09-07, the two `aurus/weighted` `humanoid-531` runs being still in flight; over the 39 of those runs the archive's own pass had timed, 36,182 worker-seconds against 79,881, 2.2x faster at the one binary. 12 of the 46 were partial and 4 were `compare`-undecided, all `humanoid-742` weighted, which is what raised the `compare` budget. Two of the 12 partial curves were re-scored at `--maximal-timeout 1800 --deadline-s 7200` to price item 3's instruction to raise the per-cut budget: `pcar-v2-888` `mrs/weighted` seed 0 reached 10 cuts in 4431 s against 9 in 1999 s, and `humanoid-742` `mrs/nsga2-apportion` seed 1 reached 4 cuts in 4705 s against 4 in 2629 s. One extra cut on one run and none on the other, for about twice the cost. A cut that exceeds 900 s exceeds 1800 s too, the late cuts' batch size being what drives the cost, so the per-cut budget was kept at 900 s.

| `maximal_solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| runs scored | 750 | 711 | 750 | 721 |
| 10 | 8.34 (0) | 7.32 (0) | 10.92 (0) | 8.43 (0) |
| 50 | 17.56 (2) | 14.55 (0) | 35.73 (2) | 27.46 (0) |
| 200 | 22.24 (15) | 16.79 (1) | 47.30 (35) | 34.21 (9) |
| 1000 | 25.71 (39) | 18.87 (22) | 54.36 (98) | 39.00 (65) |
| 7200 | 26.51 (42) | 19.21 (28) | 57.59 (133) | 39.43 (70) |

| `maximal_ideal_solutions` at cut (s) | `nsga2-apportion/mrs` | `nsga2-apportion/aurus` | `weighted/mrs` | `weighted/aurus` |
|---|---|---|---|---|
| runs scored | 750 | 711 | 749 | 721 |
| undecided | 0 | 0 | 1 | 0 |
| 10 | 0.76 (0) | 0.76 (0) | 0.82 (0) | 0.77 (0) |
| 50 | 1.59 (2) | 1.59 (0) | 1.51 (2) | 1.40 (0) |
| 200 | 1.72 (15) | 1.67 (1) | 1.72 (35) | 1.52 (9) |
| 1000 | 1.80 (39) | 1.73 (22) | 1.87 (97) | 1.63 (65) |
| 7200 | 1.80 (42) | 1.74 (28) | 1.97 (132) | 1.65 (70) |

The first caveat is the partial curves, and it did not move. 273 of the 3000 maximal curves are partial, 9.1% of 3000 and 9.3% of the 2932 runs with a maximal curve, against 280 in the archive: a cut that exceeded the 900 s per-cut budget was abandoned and the curve ends there, reaching between 2 and 15 of 20 cuts, median 6. By family, with the denominator the runs that have a maximal curve, they are `humanoid-742` on 118 of 120, `humanoid-531` 62 of 120, `pcar-v2-888` 32 of 118, `prioritized-arbiter-aurus` 22 of 107, `humanoid-503` 21 of 103, `lift` 11 of 120, `full-arbiter-aurus` 4 of 113 and `round-robin-arbiter-aurus` 3 of 120; by arm, 133 under `weighted/mrs`, 70 under `weighted/aurus`, 42 under `nsga2-apportion/mrs` and 28 under `nsga2-apportion/aurus`. The analysis reads a partial curve as undecided past its last computed cut rather than carrying its last value forward, and the bracketed `(n)` beside each cell is the number of runs so excluded at that cut. 68 runs have no maximal curve at all because their accumulator is empty, 39 under `nsga2-apportion/aurus` and 29 under `weighted/aurus`, the `aurus`-graded arms being the ones that find no repair on the hard families.

The second caveat shrank to one run. `humanoid-742` seed 11 under `weighted/mrs` on av2 has no ideal metric at any cut, `compare` having exceeded the 3000 s cap on it, and the analysis reports it as `undecided`. The archive had 205 such runs, 118 of `humanoid-742`'s 120 among them; `11305f9` on `compare`'s TLSF path and the 3000 s cap together removed all but this one. `compare` still leaves individual candidate-ideal pairs undecided inside runs it finishes, and an undecided pair counts as not ideal-implying, so every ideal count in the curves is a lower bound. The results CSV's `implies_ideal` is the run's own gate and is not affected.

Within those bounds the archive's reading holds. `weighted/mrs` holds 57.59 maximal solutions a run at 7200 s against 26.51 under `nsga2-apportion/mrs`, and the ideal-implying survivors read 1.97 against 1.80, with the four arms between 1.65 and 1.97 on that metric at every cut from 200 s on. The 340.60 gate-passing candidates a run the weighted arm accumulates, against 94.28, leave the two arms 1.97 against 1.80 on the *maximal antichain*, on a set thinned of 132 runs against 42. The surplus does not survive the antichain.

## Against the pre-registration

Outcome 3 of section 5 fired, and `PROVENANCE.json` records it as the decision. What the plan committed to on the currency held: 1000 individuals on every finished run, the ceiling never binding, and the 7200 s cap matching AuRUS's `GA_EXECUTION_TIMEOUT`. Section 7's question was answered, `humanoid-742` still capping on 107 of 120 and `humanoid-531` falling to 56. Section 10's upper bound of 1152.1 core-hours held at 786.8. Section 8's four items held on three: one scoring binary, a speedup measured at 2.2x rather than assumed, and curves kept out of the archive's tables. Section 8's last paragraph said to raise `--compare-timeout` if the calibration still showed undecided runs, and it was raised to 3000 s. Nothing here supports a claim against the paper's Table 2, the budget match being a code-level match on the weights, the six-level status ladder and the counting of an individual.

Three deviations stand, and each is recorded in `PROVENANCE.json` under `deviations`.

- **The calibration gate was not enforced.** Section 7 gates the main phase on the calibration's extrapolated cost. Both phases were enqueued together and `campaign.py`'s tick runs a declaration's phases in order without pausing, so `main` followed `calib` whatever the calibration said. The plan's 2026-09-04 amendment (`0dd5da6`) recorded that before any row existed. The ceiling given up was bounded by the archive's 1152.1 core-hours, and the campaign cost 786.8.
- **The AuRUS arm was reused, not re-run.** Section 9 re-runs the arm in the same window and screens its output for well-separation, and it was not run. On 2026-09-07 the decision was taken to score the primary against the archived reference rows in `experiments/aurus-reference/`, as the matched campaign used them, because the changes between the two campaigns were made to counter alone and nothing about AuRUS, its corpus, its settings or its hosts moved. Section 3 gave three reasons for a re-run: the shared window, the well-separation screen and a fresh sample. The first two are given up and the third was never a requirement of the test. The consequences that stand are that AuRUS's rates are unfiltered for well-separation, so the two sides remain scored by different standards on that one property, and that AuRUS's 72 lost-at-cap runs carry no verdict, so its rate on `humanoid-503` is over 0 scorable runs and that family drops out of the scorable read.
- **The maximal timeout was not raised.** Section 8 item 3 says to raise `--maximal-timeout` if partial curves survive the calibration at the fast binary, and they did, 12 of 46. The budget was kept at 900 s on the doubled-budget measurement above, one extra cut on one of two runs for twice the cost. The `compare` timeout was raised, as the item's last paragraph says to.

## Limitations

- **AuRUS's rates are unfiltered.** The well-separation screen's inputs did not survive on either host, so the hit column is used as-is; the archived record of that screen is that it moved 2 of 25 family rates, `lift` 0.167 to 0.300 and `lily11` 0.900 to 0.833.
- **A kill is not symmetric.** counter's kills score as zero and AuRUS's drop out. Both reads are given and the registered one is the primary.
- **A solution means two things.** counter's curve counts gate-passing candidates with sub-second timestamps; AuRUS's counts the running length of `ga.solutions` off its per-iteration log line, dated to the second and moving only at a generation boundary. The 415.62 solutions a run AuRUS reaches at 7200 s over its 692 shared-family runs, against counter's 61.64 to 340.60 per arm, are counts of different things.
- **Every ideal count in the curves is a lower bound.** One run is undecided outright, and finished runs carry undecided pairs inside them.
- **273 maximal curves are partial** and 68 runs have none, concentrated in the families that accumulate most, and the late cuts of the weighted arms are read over the thinned set.
- **Nothing here pairs against the archive.** The two campaigns are different engines, and section 2 registered that no row here pairs with an `aurus-matched` row on `(spec, seed)` and that no result here is attributable to the filter speedup alone, the binaries differing by more than that one commit.
- **The AuRUS rows predate every counter row here.** The plan's shared window was given up with the re-run, and the arm's rows are the ones the matched campaign scored against.
- **One weight setting and one population.** All four arms run 0.1 / 0.2 / 0.7 at population 100 single-threaded, so the selection contrast here and the selection-smoke null are at different populations and thread counts and neither reproduces the other.

## What is owed

A `score` phase kind in `campaign.py`, so the maximality pass is declared in `campaign.toml`, launched and resumed by the tick, gated on the scoring binaries' freshness, and recorded in a per-host manifest the `maximality_pass` block can be assembled from. Two campaigns have now run the pass by hand, 716.2 and 311.7 worker-hours, each time the whole RQ3 result, through two shell scripts a host and a block assembled afterwards from the timings, failures and version files. It is in progress on `feat/campaign-score-phase` at close.

The survivor-block skip in `score_curves.py`'s maximality pass, carried over from the archive. Each cut re-decides from cold the survivor-against-survivor pairs the previous cut already found non-dominating; removing them is exact, roughly halves the pass, and is what would bring the 273 partial curves within the per-cut budget on every family but `humanoid-742`. The doubled-budget measurement is the evidence that raising the budget does not.

A pairwise-relation flag on `compare`, carried over from the archive, for the incremental antichain.

A re-run of the AuRUS arm with the well-separation screen's inputs kept, as section 9 specified, should the unfiltered-rate caveat ever need closing. Nothing about AuRUS changed, so the archive's rows stand until then.

A re-score of the one `compare`-undecided run, `humanoid-742` seed 11 under `weighted/mrs`, should anything need that family's ideal curve complete; it is one `compare` call at a longer cap.

The speedup bought back 365.3 core-hours and 123 runs from the cap, and the pooled contrast it was expected to move did not move. What it did change is the standing of the null, which is now the third reading of the same contrast and the first taken under a rule written before the rows existed.
