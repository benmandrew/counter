# Selection scheme crossed with status grading

Sweep G crossed `selection_scheme` (`nsga2-apportion` against `weighted`) with `fitness.status_grading` (`mrs` against `aurus`) over 25 families and 6 seeds, 600 runs from one binary at `1816e27`. Every arm ran `generations = 500` under a 400 s `genetic.max_wall_s`, so the deadline rather than the generation count ended each run and the four arms shared a wall clock. The harness capped each run at 3600 s from outside.

`PLAN.md` section 2 registers no decision rule, no primary endpoint, no alpha and no control arm. Every p-value below is therefore **post-hoc**: it describes this sample and tests no hypothesis, because the contrast, the metric and the time cut were all free when they were chosen.

## Both checks pass

`stopped_by` reads `deadline` on all 355 manifests and `generations` on none, so no family reached 500 generations inside 400 s and the arms share a time axis. `implies_ideal` reads 184 of 600 (30.7%) and no arm is flat zero, so the `compare` join is sound.

## The endpoint says one thing and the accumulator says another

Read from the results CSV, the selection factor looks decisive. Over 300 paired runs `nsga2-apportion` leads `weighted` 191 to 9 on `found_repair` and 145 to 5 on `implies_ideal`, both at exact McNemar p = 0.0000.

That reading does not survive the censoring. 245 of the 600 runs were killed at the 3600 s external cap, and the kills are not spread evenly: `weighted` was killed on 214 of the 300 paired runs against `nsga2-apportion`'s 31. A killed run writes no manifest, so its `implies_ideal` reads zero whatever it found, and the endpoint contrast is confounded with the thing that produced it.

The accumulator flushes every gate-passing candidate as the run proceeds, so it records what the killed runs found. It reverses the picture:

| arm | solutions by 400 s | `implies_ideal` | killed at cap | mean wall |
|---|---|---|---|---|
| `nsga2-apportion/mrs` | 310.6 | 79/150 | 20/150 | 1453 s |
| `nsga2-apportion/aurus` | 162.0 | 83/150 | 11/150 | 889 s |
| `weighted/mrs` | 3310.8 | 5/150 | 126/150 | 3178 s |
| `weighted/aurus` | 1917.9 | 17/150 | 88/150 | 2527 s |

`weighted/mrs` accumulates 10.6 times what `nsga2-apportion/mrs` does and delivers a sixteenth as many ideal-implying repairs.

## The weighted arms do not fail to search

Time to first repair is flat across all four arms: a median of 5.6 to 6.4 s, reached on 95.3% to 99.3% of runs. No arm is slow to find something.

What separates them is what happens after the deadline. `genetic.max_wall_s` bounds the generation loop and nothing after it, and the work after it is quadratic in what the search accumulated: `compute_subsumed` in `src/filter/implication.cpp` sweeps pairs over the accumulated pool at the final implication filter. A `weighted/mrs` run arrives at that filter holding 3341 candidates, which is about 5.6 million pairwise implication queries, each up to two solver calls, begun after its 400 s budget has already been spent. The external cap is what ends those runs, and it ends them before they report.

So the finding is not that scalarisation searches badly. It is that scalarisation searches prolifically into a post-processing stage priced in the square of its own output.

## The grading factor moves the same lever

`aurus` grading beats `mrs` on both columns — `found_repair` 47 to 9 (p = 0.0000), `implies_ideal` 23 to 7 (p = 0.0052) — and is killed less often, 99 against 146 of 300. It also accumulates roughly half as much: 162.0 against 310.6 solutions by 400 s under NSGA-II, 1917.9 against 3310.8 under `weighted`. The arm that accumulates less finishes more often, which is the same mechanism read through the other factor.

## What the selection contrast may not claim

AuRUS's published weights (0.1 syntactic, 0.2 semantic, 0.7 status) are set globally and only the `weighted` scheme reads them; both NSGA-II arms rank the three objectives separately and ignore the triple. `PLAN.md` section 7 recorded this before the run. The selection factor is therefore NSGA-II against AuRUS-weighted *scalarisation*, not NSGA-II against weighting in general, and separating the two would need a third level at some other weight triple that this campaign does not run.

## Two of the six metrics, and why

`solutions` and `time_to_first_repair` are measured over all 600 runs. They come from `accumulated/index.tsv` and need no solver call, which is also what makes them readable on the 245 censored runs where the endpoint is not.

`ideal_solutions` and `time_to_first_ideal_repair` are not measured. Each needs one `compare` call per run, about 118 core-hours over this corpus, and both lab hosts were committed to the 2026-08-29 campaign's maximality pass at the time of the close. They are owed, and they are the two that would settle whether the weighted arms' accumulated candidates contain ideal repairs that the cap prevented them from reporting. **They were measured on 2026-09-03; see the addendum below, which supersedes the two paired contrasts above.** The sentence stands as written because it is what the close said at the time.

`maximal_solutions` and `maximal_ideal_solutions` are not measurable here, and the arithmetic rather than the budget is the reason. The 863,531 accumulated candidates give 5,706,083,502 pairwise implication comparisons at 20 log-spaced cuts, 20.9 times the 2026-08-29 campaign, which is itself a multi-day job across two 32-core hosts. 91.7% of those candidates sit in the weighted arms, so the arm whose behaviour most wants a maximality reading is precisely the one whose reading cannot be computed. A later curve campaign that expects a prolific arm should cap `accumulate_repairs` or budget for this before it launches rather than after.

## Closing note

This campaign was closed on 2026-09-02, four days after its runs finished. It had no `PROVENANCE.json` and no `REPORT.md`, and its 600 rows and 3.5 GB of run directories were found inside a git worktree that had been removed after an unrelated pull request merged — a directory `git worktree list` no longer names. Nothing was lost, because git leaves ignored files behind when it removes a worktree, and the data now sits in the main checkout beside every other campaign's. The gap it leaves is in the record rather than the data: a campaign that produces a mechanism this clean should not have spent four days as an unlisted directory.

## Addendum, 2026-09-03: the schemes separate on delivery, not on reach

The two metrics listed above as owed were measured on 2026-09-03 over all 600 runs. 600 of 600 scored with 0 failures, giving 900,946 curve rows, of which 35,860 are `ideal_solutions` and 600 are `time_to_first_ideal_repair`, one per run. All four arms hold exactly 150 runs. Both metrics score what a run's accumulator held, so a run killed at the external cap before it could filter and report is measured on the same footing as one that finished.

| arm | found an ideal at any time | mean peak `ideal_solutions` | recorded `implies_ideal` |
|---|---|---|---|
| `nsga2-apportion/mrs` | 88/150 | 21.9 | 79/150 |
| `nsga2-apportion/aurus` | 84/150 | 15.3 | 83/150 |
| `weighted/mrs` | 87/150 | 106.2 | 5/150 |
| `weighted/aurus` | 85/150 | 93.1 | 17/150 |

32 of the 600 runs are undecided rather than censored, `compare` having timed out before it could rank what they held, and `PLAN.md` section 6 keeps that third state out of the second rather than folding it in. Held out, and paired on the (spec, seed) cell, the selection factor is 14 to 18 over 277 pairs, exact McNemar p = 0.5966, and the grading factor is 15 to 7 over 281 pairs, p = 0.1338. Neither level of the other factor moves either: selection reads 8 to 11 within `mrs` (p = 0.6476) and 6 to 7 within `aurus` (p = 1.0000), grading 6 to 3 within `nsga2-apportion` (p = 0.5078) and 9 to 4 within `weighted` (p = 0.2668). These figures are **post-hoc** for the same reason every p-value above is, `PLAN.md` registering no endpoint and no decision rule. The recorded contrast they replace was 145 to 5 in `nsga2-apportion`'s favour at exact McNemar p = 0.0000.

The undecided cells fall 13 in `weighted/mrs` and 9 in `weighted/aurus` against 5 in each NSGA-II arm, tracking the accumulated pool size that makes a `compare` call slow. Folding them in as failures therefore charges the prolific arm for its own slow scoring, and read that way the two factors give 19 to 19 (p = 1.0000) and 16 to 10 (p = 0.3269). That changes no conclusion here, both readings being flat, and it is the reading error section 6 exists to prevent in a campaign whose contrast is not.

Median time to first ideal repair is 5.7 s to 7.1 s across all four arms, beside the 5.6 s to 6.4 s already recorded for time to first repair. Reaching an ideal-implying candidate costs about a second more than reaching any repair, on every arm.

Nine families whose recorded `implies_ideal` was flat 0/6 on both weighted arms read 6/6 here on at least one of them: detector-aurus, lily02, lily11, ltl2dba-r-2, ltl2dba-theta-2, ltl2dba27, rg2, round-robin-arbiter-aurus and simple-arbiter-aurus. `lily15` runs the other way, at 4/6 for `weighted/aurus` and 5/6 for `weighted/mrs` against 1/6 for `nsga2-apportion/aurus` and 0/6 for `nsga2-apportion/mrs`. Five families stay 0/6 on all four arms: gyro-var1, humanoid-458, humanoid-503, lift and pcar-v2-888.

The mechanism identified above is unchanged, and it is now the entire account. The post-deadline implication filter is quadratic in what the search accumulated, `genetic.max_wall_s` bounds the generation loop and not that filter, and `weighted/mrs` accumulates 10.6 times what `nsga2-apportion/mrs` does. No reading of the recorded `implies_ideal` as a difference in what the schemes *find* survives these two metrics. The schemes differ in delivery rather than in reach.

The two metrics answer different questions, and both are legitimate: the recorded endpoint scores what a run reported inside the external cap, and these score what its accumulator held. The four arms separate on the first and not on the second, which leaves the maximality reading the close could not compute exactly where the close left it. A metric that a censored run can still be scored on is worth the core-hours it costs, and this campaign is on the record twice because it was first read without one.

The scoring pass ran locally on 2026-09-03, 00:06:17 to 03:21:21, and its `warnings.log` accounts for itself exactly: 245 runs with no `run.json` to read, which are the ones the external cap killed, and 32 `compare` timeouts, which are the undecided. Two provenance gaps go with it. The `compare` it used reports `commit=a10ecbc` and `dirty=1` and was staged into a scratchpad, so it is neither the campaign's binary at `1816e27` nor a reconstructible tree; the ideals came from a second worktree, whose `examples/` differs from this branch only at `mode-arbiter`, a FRETISH subject outside this corpus.

What licenses the newer binary is the agreement rather than the argument. On the 355 runs that reported a manifest, the reach derived here matches the `implies_ideal` the campaign's own binary computed at `1816e27` on 355 of 355, with no disagreement in either direction. Of the 245 killed runs, 160 had already found an ideal-implying candidate and 53 had not.
