# Selection scheme crossed with status grading: what each arm finds as a function of time

Pre-registered 2026-08-28, before any row of this campaign existed. Section 2 is the one a reader arriving from another plan in this archive needs first: this campaign registers no decision rule and no primary endpoint, deliberately, and everything after section 2 is written on that footing.

## 1. Question

counter ranks candidates by a *selection scheme* and scores their realizability on a *status grading* scale. Both were chosen against AuRUS, the tool this search is derived from, and neither has been measured as a function of elapsed time. Every campaign archived under `experiments/` stops at a fixed generation count, so its rows describe what an arm finds in 10, 40 or 500 generations rather than what it finds in a fixed wall-clock budget.

Two arms that stop at different moments answer different questions unless the response is read as a function of time. A generation costs each scheme a different amount of wall time: `experiments/2026-08-26-selection-smoke` measured `weighted` at 2.67x the wall time of `nsga2-apportion` on 21 of 21 families, with yield and quality both null at p = 0.61. A generation-matched comparison of those two schemes is therefore not budget-matched, and the campaign that measured them could not say which of the two finds more per second.

The campaign asks what each of four configurations finds over a shared 400 s search budget, and reports the answer as curves.

## 2. No decision rule, and no primary endpoint

This campaign registers no decision rule and no primary endpoint. It reports curves, and the curves are the whole deliverable. Nothing here fixes an alpha, a statistical test, or an accept/reject outcome, and no arm is nominated as a control.

Every other plan in this archive binds a rule — sweep O in `2026-08-20-ops-grammar` takes one of four outcomes, sweep U in `2026-08-26-assumption-reach` one of three — so a later reader will look for that section and must be told plainly that it does not exist. There is no decision rule.

The consequence follows directly. Any p-value computed from these rows is **post-hoc** and has to be labelled as such wherever it is written down, in `REPORT.md`, in `PROVENANCE.json` and in anything downstream of either. A test chosen after the curves are drawn describes this sample and tests no hypothesis, because the contrast, the metric and the time cut were all free when the analyst picked them. A comparison that wants an inferential claim pre-registers one and re-runs the arms; this campaign is what tells that comparison where to look.

## 3. What runs

A 2x2 over the *Temporal Logic Synthesis Format* (TLSF) corpus, crossing `genetic.selection_scheme` against `fitness.status_grading`, at 25 families and seeds 0 to 5. Four arms, 600 runs.

| arm | `selection_scheme` | `status_grading` |
|---|---|---|
| 1 | `nsga2-apportion` | `mrs` |
| 2 | `nsga2-apportion` | `aurus` |
| 3 | `weighted` | `mrs` |
| 4 | `weighted` | `aurus` |

`generations = 500` sits far above what any of these families reaches, and `genetic.max_wall_s = 400` bounds every run from inside, so the deadline rather than the generation count is what ends each run and every arm gets the same wall clock. counter stops itself at the next generation boundary past the deadline and writes `run.json` and its repairs normally, so a run that spends its whole budget is measured rather than lost. The harness's external `timeout_caps` stay at 3600 s as a backstop against a hung tool.

The rest of the configuration is fixed across all four arms and is the shipping one: `runtime.parallel = 1`, `filters.run_implication = true`, `genetic.accumulate_repairs = true`, `[tlsf] repair_mode = "monolithic"`, the log similarity metric, and the weakening screen off. `accumulate_repairs` is what makes a curve readable at all, each accumulated candidate carrying the generation and the elapsed time it was found at.

## 4. Why the runs are single-threaded

`runtime.parallel = 1` matches AuRUS, whose genetic algorithm is single-threaded at the pinned base commit this project benchmarks against. Comparing a 32-thread counter against a 1-thread AuRUS on a time axis would measure the thread count.

The second reason binds inside this campaign rather than across it. The endpoint is wall time, and a scoring pool sized from the host's `--jobs` would make every measurement a property of the machine that ran it instead of the arm that was configured. The two hosts run at `jobs = 16`, so the thread budget per run is stated in the config rather than derived from load.

## 5. The six metrics

All six come from `<run-dir>/accumulated/index.tsv` through `scripts/score_curves.py`, which reads the accumulator's flushed record of every candidate that passed the output gate and when. Four are curves against elapsed time and two are scalars.

- **`solutions`** — gate-passing candidates found by time t.
- **`ideal_solutions`** — of those, the ones equivalent to or strictly stronger than an ideal under `compare`.
- **`time_to_first_repair`** — elapsed seconds to the first gate-passing candidate.
- **`time_to_first_ideal_repair`** — elapsed seconds to the first ideal-implying one.
- **`maximal_solutions`** — the maximal antichain of the set found by time t.
- **`maximal_ideal_solutions`** — of those survivors, the ideal-implying ones.

The last two are computed offline, after and apart from the timed run, at a bounded number of log-spaced time cuts. `maximal` is a pairwise implication sweep that has taken 19 GB on this corpus, so running it inside a timed run would put its cost into the measurement it is meant to describe. Their cost never enters a measured time.

## 6. Censoring

A run that never finds an ideal-implying repair has no time to one. `score_curves.py` writes every such value as an empty field with an explicit `censored` flag, never as a zero and never as an omitted row, so a *right-censored* run survives into the analysis instead of reading as an instantaneous success or vanishing from the denominator.

Three states are distinguished rather than two: the event happened, the event did not happen within the budget, and the event could not be decided because `compare` timed out. The third is unknown and is not folded into the second. Pooling only the runs where the event happened would reward whichever arm fails on its own slow cases, which is the reading error the flag exists to prevent.

## 7. One confound, stated rather than fixed

AuRUS's published weights — 0.1 syntactic, 0.2 semantic, 0.7 status — are set globally through `gen_configs.py --weights`, and only the `weighted` scheme reads them. Both NSGA-II arms rank the three objectives separately and ignore the triple entirely, so the weights ride with two of the four arms by construction.

The selection factor is therefore "NSGA-II against AuRUS-weighted *scalarisation*", not "NSGA-II against weighting in general", and the write-up has to say so in those words. Separating the two would need a third level of the selection factor at some other weight triple, which this campaign does not run. Stating the confound before the run is what keeps the narrower claim available afterwards.

## 8. Why the factors are crossed

`status_grading` changes what the status objective *is*, and the two schemes consume that objective differently. Under NSGA-II it is one of three separately ranked objectives, where only its order matters. Under `weighted` it is a number scaled by 0.7 into a scalar, where its spacing matters as much as its order.

A six-level ladder and a three-point scale are the same order and different spacings, so a grading change that is nearly inert under NSGA-II can move the weighted arm a long way. A 2x2 can show that interaction; two one-factor studies run at whichever scheme was default cannot, and would attribute the interaction to whichever factor happened to be varied.

## 9. The corpus: all 25 families

The corpus is `H2H_TLSF_READY`, all 25 families, rather than the 21 of `SELECTION_SMOKE_SPECS`. That constant's four exclusions — `humanoid-503`, `humanoid-531`, `humanoid-742` and `pcar-v2-888` — were a pure cost rule, dropping families whose mean wall time in the `2026-08-23-monotone` `monoon` arm ran above 600 s, with `humanoid-742` capping at 7200 s on every seed and reading 0 of 20 on `found_repair`.

`genetic.max_wall_s` now bounds every run at 400 s from inside, so the reason for the exclusion is gone and it is retired here. An expensive family contributes censored observations at a known cost rather than an unbounded bill, which is exactly what section 6's flag is for. Whether those four families find anything inside 400 s is itself unknown and worth a row.

## 10. What `aurus` grading is

`fitness.status_grading = "aurus"` is a faithful reproduction of AuRUS's six-level ladder: 0.00 for both sides unsatisfiable, 0.05 for a satisfiable guarantee side alone, 0.10 for a satisfiable assumption side alone, 0.20 for two sides that contradict each other, 0.50 for jointly satisfiable and unrealizable, and 1.00 for realizable. Both side queries are asked whatever the first answers, as AuRUS asks them.

It is a third scale rather than a re-use of counter's `tiered`, which is the closest existing thing and is not the AuRUS scale: `tiered` has three points where the ladder has six, and the four levels below 0.50 are the whole distinction the ladder draws.

One divergence is deliberate and is recorded here rather than discovered later. `status_score_aurus` does not fold the well-separation query in behind its realizability query, where counter's own two scales do, because AuRUS never checks well-separation anywhere in its search. So the `aurus` arms pay `k_status_realizable` to a candidate realizable only by defeating its own assumptions, and the `mrs` arms do not. That is a property of the design being reproduced, and removing it would make the arm a reproduction of nothing.

## 11. Budget

No run finishes before its deadline at `generations = 500`, so the cost is exactly specs x arms x seeds x cap: 25 x 4 x 6 x 400 s = 66.7 h of run time against a budget of 80 h. At `jobs = 16` on each of av2 and av3, that is 32 concurrent runs and about 2.1 h of wall clock. The remaining 13.3 h covers the post-deadline work the cap does not interrupt, the deadline stopping the search and leaving the filters, the scoring and the final realizability gate to finish.

Six seeds is what the budget buys rather than what a power calculation asks for, this campaign reporting curves. Topping up later re-runs nothing: `run_experiments.py` resumes off the results CSV key, and `(spec, seed)` is in it, so added seeds cost only themselves. The `jobs = 16` proposal is unverified until the `curves-calib` phase reads a peak resident set back, that value having been sized for runs whose own thread pool was hardware concurrency.

## 12. What would make the results unreadable

Two checks come before anything is drawn from these rows, and both are cheap.

**`ideal_solutions` at zero across every arm.** `score_curves.py` joins `compare`'s per-repair output back to the accumulation index by file name, and a broken join reads as a search that found no ideal-implying candidate anywhere. The archived rates rule that out as a search result — the monotone campaign found ideal-implying repairs on most of these families — so a flat zero points at the join first and at the search second. Cross-check it against the `implies_ideal` column of the results CSV, which is derived from a separate `compare` call per run.

**`stopped_by` not reading `deadline`.** Every row should carry it, the whole design resting on the deadline rather than the generation count ending each run. A row that stopped on generations means that family reached 500 of them inside 400 s, and a row that stopped any other way was not budget-matched. If `stopped_by` is not essentially uniform, the time axis is not shared across arms and no curve comparison on it holds.

## 13. Provenance

Branch `campaign/selection-grading`, declared in `campaign.toml` beside this file, which carries the per-host seed split (av2 takes 0 to 2, av3 takes 3 to 5) so no range is ever chosen at a prompt. The profiles are `curves-calib` and `curves` in `scripts/run_experiments.py`; the calibration is run once by hand, its timings fix the seed count, and only then is this campaign enqueued. Both arms of every contrast run on the same host at the same seed, so a host difference cancels inside the `(spec, seed)` cell.

On close, vendor `gen_configs.py`, `run_experiments.py`, `merge_experiments.py` and `score_curves.py` into `scripts/` beside this plan with their blob shas in `PROVENANCE.json`, and record whether the branch merged, was split or was rebased. `PROVENANCE.json` records the absence of a decision rule in the same field the other archives use to record the decision taken, since a blank there would read as an unfinished close rather than a design.

Registering the absence of a rule is a stranger act than registering one, and it is the only thing that stops a curve campaign from acquiring a decision rule after its curves have been drawn. The other plans in this archive can be checked against their outcomes; this one can only be checked against what it declined to promise.
