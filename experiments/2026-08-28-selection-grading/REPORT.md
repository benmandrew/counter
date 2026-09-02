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

`ideal_solutions` and `time_to_first_ideal_repair` are not measured. Each needs one `compare` call per run, about 118 core-hours over this corpus, and both lab hosts were committed to the 2026-08-29 campaign's maximality pass at the time of the close. They are owed, and they are the two that would settle whether the weighted arms' accumulated candidates contain ideal repairs that the cap prevented them from reporting.

`maximal_solutions` and `maximal_ideal_solutions` are not measurable here, and the arithmetic rather than the budget is the reason. The 863,531 accumulated candidates give 5,706,083,502 pairwise implication comparisons at 20 log-spaced cuts, 20.9 times the 2026-08-29 campaign, which is itself a multi-day job across two 32-core hosts. 91.7% of those candidates sit in the weighted arms, so the arm whose behaviour most wants a maximality reading is precisely the one whose reading cannot be computed. A later curve campaign that expects a prolific arm should cap `accumulate_repairs` or budget for this before it launches rather than after.

## Closing note

This campaign was closed on 2026-09-02, four days after its runs finished. It had no `PROVENANCE.json` and no `REPORT.md`, and its 600 rows and 3.5 GB of run directories were found inside a git worktree that had been removed after an unrelated pull request merged — a directory `git worktree list` no longer names. Nothing was lost, because git leaves ignored files behind when it removes a worktree, and the data now sits in the main checkout beside every other campaign's. The gap it leaves is in the record rather than the data: a campaign that produces a mechanism this clean should not have spent four days as an unlisted directory.
