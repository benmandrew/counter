# counter against AuRUS on a matched search budget

Pre-registered 2026-08-29, before any row of this campaign existed. Section 2 is the one a reader arriving from another plan in this archive needs first: this campaign registers no decision rule and no primary endpoint, and everything after section 2 is written on that footing.

## 1. Question

counter's search budget is a count of `generations` and AuRUS's is a count of individuals, so the two tools have never been stopped by the same rule. The `2026-08-14-aurus-h2h` campaign gave counter 10 generations at population 200 against AuRUS's own stopping rule, which is roughly twice the individuals; the `2026-08-28-selection-grading` campaign gave counter a 400 s deadline against nothing at all. Neither says which tool finds more repair per unit of search.

Three keys landed on 2026-08-29 to close that gap — `[genetic] termination`, `max_individuals` and `max_wall_s` — and this campaign is the first use of them. Phase 1 fixes the budget in AuRUS's currency and lets wall time vary, so wall time becomes the measured outcome. Phase 2 fixes AuRUS's two-hour deadline and lets the individual count vary, on the six families where a 400 s horizon truncates AuRUS's own discovery curve.

The deliverable is the same set of curves the 2026-08-28 campaign produces, read against the archived AuRUS logs on a shared axis.

## 2. No decision rule, and no primary endpoint

This campaign registers no decision rule and no primary endpoint. It reports curves and wall times, and those are the whole deliverable. Nothing here fixes an alpha, a statistical test, or an accept/reject outcome, and no arm is nominated as a control.

Every other plan in this archive binds a rule — sweep O in `2026-08-20-ops-grammar` takes one of four outcomes, sweep U in `2026-08-26-assumption-reach` one of three — so a later reader will look for that section and must be told plainly that it does not exist. There is no decision rule.

Any p-value computed from these rows is **post-hoc** and has to be labelled as such wherever it is written down, in `REPORT.md`, in `PROVENANCE.json` and in anything downstream of either. A test chosen after the curves are drawn describes this sample and tests no hypothesis, the contrast and the metric and the time cut all having been free when the analyst picked them.

## 3. AuRUS's stopping rule

AuRUS stops at 1000 bred individuals, and the figure is established three ways rather than one.

**The code.** `GeneticAlgorithm.java` increments `numberOfVisitedIndividuals` at exactly two sites: once per accepted mutant, guarded by `!chromosome.equals(mutated)`, and once per crossover offspring distinct from its first parent. Both sites sit inside the breeding loops and run before the population is scored, and `checkTermination()` is tested between offspring, so a run terminates mid-generation.

**The archive.** All 679 archived head-to-head logs carrying a settings banner read `GA_MAX_NUM_INDIVIDUALS=1000, GA_POPULATION_SIZE=100, GA_EXECUTION_TIMEOUT=7200`.

**The paper.** Brizzio et al., "Automated Repair of Unrealisable LTL Specifications Guided by Model Counting", GECCO '23, section 6: "The termination criterion is reached either when 1000 individuals are generated or after 2hrs of execution time."

The 1000 is a cap on bred individuals and never on solutions. Over the 600 archived runs printing a final solution count, solutions per run read median 486, p90 637 and maximum 741, and zero runs reached 1000. Reading the cap as a solution limit would make every one of those runs look budget-bound when none of them was.

counter's `SearchBudget` (`include/genetic/pipeline.hpp`) matches the code on both points: an offspring counts only where it differs from the parent it was bred from, and the budget is checked inside `breed_offspring` between slots rather than at the generation boundary.

## 4. Phase 1: the individuals-matched cross, profile `matched`

The 2x2 of the 2026-08-28 campaign, run again in AuRUS's currency. `genetic.selection_scheme` crosses `fitness.status_grading` over the 25 families of `H2H_TLSF_READY` at seeds 0 to 5. Four arms, 600 runs.

| arm | `selection_scheme` | `status_grading` |
|---|---|---|
| 1 | `nsga2-apportion` | `mrs` |
| 2 | `nsga2-apportion` | `aurus` |
| 3 | `weighted` | `mrs` |
| 4 | `weighted` | `aurus` |

Three flags carry the match. `genetic.termination = "individuals"` and `max_individuals = 1000` are AuRUS's rule; `population_size = 100` is `GA_POPULATION_SIZE` from the same banner. The population matters as much as the cap, since 1000 individuals at population 200 is half the generations of 1000 at population 100.

`generations = 500` is a ceiling that must not bind. counter breeds `population_size - elite_n = 90` offspring a generation at `elitism_rate = 0.1`, and counts only those differing from their parent, so 1000 individuals needs at least 12 generations. The external `timeout_caps` sit at 7200 s, matching AuRUS's `GA_EXECUTION_TIMEOUT`, and should not bind either.

The rest of the configuration is the shipping one and is fixed across all four arms: `runtime.parallel = 1`, `filters.run_implication = true`, `genetic.accumulate_repairs = true`, `[tlsf] repair_mode = "monolithic"`, the log similarity metric, and the weakening screen off. Single-threaded runs match AuRUS and keep the endpoint off the host, wall time being what is measured.

## 5. Why the 2026-08-28 campaign cannot answer this

That campaign gave every arm a 400 s `genetic.max_wall_s`. Its 355 surviving manifests read a median 81 generations run at population 200, with p90 256 and a maximum of 461 against a 500-generation ceiling. At 180 offspring a generation that median is about 16,200 bred individuals, roughly 16x AuRUS's budget.

A 400 s window therefore hands counter an order of magnitude more search than AuRUS is allowed. Any claim that counter found more inside 400 s is a claim about the budget rather than about the search, and is not defensible as written. Phase 1 removes that asymmetry by fixing the budget and measuring the time.

## 6. Phase 2: the time axis, profile `matched-long`

Six families — `humanoid-503`, `prioritized-arbiter-aurus`, `full-arbiter-aurus`, `humanoid-531`, `humanoid-458` and `pcar-v2-888` — at the same four arms and the same seeds 0 to 5. 144 runs.

These runs are generation-bounded again, with `genetic.max_wall_s = 7200` matching `GA_EXECUTION_TIMEOUT`, at `population_size = 200`. That population is the operating point the 400 s `curves` rows this phase will be read beside were measured at, so the two sets of curves share an axis. The external cap is 14400 s, double the deadline, for the post-deadline gate and filter the deadline does not interrupt.

## 7. Why those six families

The selection comes from the 780 archived AuRUS run logs, by pairing each generation row's `#Sol` field with the `Elapsed Time` line that follows it, which bounds the first solution above at the generation boundary. Of those runs, 698 ever solve. Among the solvers the median first solution is 5.0 s and the p90 is 989 s, and 87.1% arrive within 400 s. On 19 of 26 families the median first solution is under 30 s.

The 90 solving runs that need longer than 400 s are almost entirely these six. `humanoid-503` is the sharp case: it solves 30 of 30 runs, at a median first solution of 2425 s, so a 400 s window reports it at 0%. Spending two hours a run on the other 19 families would buy curve where the 400 s rows already have it.

AuRUS's seventh such family, `humanoid-741`, is deliberately absent. `H2H_TLSF_READY` carries `humanoid-742` instead, and adding a family to that list would change what every profile reading it means, so it is left out rather than smuggled in.

## 8. What the paper does not fix, and what must not be claimed against it

Three gaps between the paper and the code are recorded here rather than discovered during analysis.

**The weights.** The paper does not fix one configuration. It names (α=.7, β=.1, γ=.2) among three that "typically reach better performance", and reports its Table 2 "with the best-performing configuration for each case study" — per case, not once globally. The archived AuRUS runs used one global setting, as counter does here through `--weights 0.1 0.2 0.7`, so the head-to-head is symmetric between the two tools. Neither side reproduces the paper's table, and no claim in `REPORT.md` may be made against it.

**The status ladder.** The paper describes a five-level ladder (1, 0.5, 0.2, 0.1, 0). `ModelCountingSpecificationFitness.java` has six, inserting 0.05 for `SPEC_STATUS.GUARANTEES`. counter's `fitness.status_grading = "aurus"` reproduces the code's six, so it matches what the archived runs did and diverges from the published description by one level.

**The individual count.** The paper is silent on how an individual generated is counted. It does not say the counter increments once per mutant and once per crossover offspring, nor that an offspring equal to its parent is excluded. counter's `SearchBudget` matches the code on both, so the budget match is a code-level match and has to be described as one.

## 9. AuRUS's timeout does not fire

Of the 780 archived AuRUS runs, 173 (22.2%) were killed by the harness at 7500 s, concentrated in six hard families. The internal `GA_EXECUTION_TIMEOUT = 7200` printed its `GENETIC ALGORITHM TIMEOUT REACHED` message in 0 of those 780 logs.

The reason is structural. `checkTermination()` sits between offspring in the breeding loops and cannot interrupt the `parallelStream()` fitness evaluation of the whole population, so a run stuck in scoring overruns its own deadline and is killed from outside. The paper's Table 2 reports 7400 s for five cases against its stated 7200 s cap, which is the same behaviour surfacing in the published numbers.

counter's `max_wall_s` has the matching limit and it is recorded in the root `CLAUDE.md`: nothing interrupts filters, scoring or the final gate, so a run overruns by whatever the generation it was in had left. Both tools therefore overshoot their internal deadline, for the same reason, and phase 2's external cap is what bounds each.

## 10. The six metrics

All six come from `<run-dir>/accumulated/index.tsv` through `scripts/score_curves.py`, which reads the accumulator's flushed record of every candidate that passed the output gate and the elapsed time it was found at. Four are curves against elapsed time and two are scalars.

- **`solutions`** — gate-passing candidates found by time t.
- **`ideal_solutions`** — of those, the ones equivalent to or strictly stronger than an ideal under `compare`.
- **`time_to_first_repair`** — elapsed seconds to the first gate-passing candidate.
- **`time_to_first_ideal_repair`** — elapsed seconds to the first ideal-implying one.
- **`maximal_solutions`** — the maximal antichain of the set found by time t.
- **`maximal_ideal_solutions`** — of those survivors, the ideal-implying ones.

The last two are computed offline under `--maximality`, after and apart from the timed run, at a bounded number of log-spaced time cuts. `maximal` is a pairwise implication sweep that has taken 19 GB on this corpus, so running it inside a timed run would put its cost into the measurement it describes. Their cost never enters a measured time.

Censoring follows the 2026-08-28 campaign unchanged. A run that never finds an ideal-implying repair has no time to one, and `score_curves.py` writes every such value as an empty field with an explicit `censored` flag rather than a zero or an omitted row.

## 11. Verification checks

Five checks come before anything is drawn from these rows. Each is cheap, and each catches a way the campaign silently measures something other than what it declares.

1. **`stopped_by` must read `individuals` on essentially every phase-1 row.** A row reading `generations` means the 500-generation ceiling bound instead of the individuals cap, and that row measures a different budget. `stopped_by`, `generations_run` and `individuals_bred` are in the manifest at schema 21 and are the fields to read.
2. **`individuals_bred` should sit at or just above 1000 on phase-1 rows.** The budget is checked between offspring slots inside `breed_offspring`, so a small overshoot within one slot is expected. A large one is a bug in the budget wiring.
3. **A flat `ideal_solutions` at zero across every arm points at the join first.** `score_curves.py` joins `compare`'s per-repair output back to the accumulation index by file name, and a broken join reads as a search that found nothing ideal-implying anywhere. Cross-check against the `implies_ideal` column of the results CSV, which comes from a separate `compare` call per run.
4. **Phase 1 should be cheap and phase 2 should not.** If phase-1 runs approach their 7200 s cap, the assumption that a 1000-individual budget leaves a small accumulated set to filter is wrong, and phase 2's cost estimate is wrong with it.
5. **Count the censored runs of both phases before any analysis.** A censored run is a run directory holding `accumulated/index.tsv` with no `run.json`. In the 2026-08-28 campaign 245 of 600 runs were censored at a 3600 s cap, which is why the caps here are 7200 s and 14400 s.

## 12. Cost

Phase 1 is expected cheap. 1000 individuals is roughly 25 s of search at the rate the 2026-08-28 manifests recorded, so the 600 runs come to about 4.2 h of search in total, and the post-search realizability gate and implication filter dominate the bill rather than the search itself.

Phase 2 is the uncertain half, and the 2026-08-28 campaign is the reason to say so plainly. That campaign's cost estimate was low by a factor of 2.2, because `genetic.max_wall_s` bounds the generation loop and nothing after it — the final realizability gate, the weakening screen and the implication filter all run past the deadline uncapped. The same correction has not been validated at a 7200 s deadline, where the accumulated set the filters walk is far larger, so phase 2's cost is to be read off its first ~40 rows rather than predicted from phase 1.

The ceiling on phase-2 search alone is 144 x 7200 s = 288 h, which is 18 h of wall clock at `jobs = 8` on each of av2 and av3. The available budget is roughly 68 h of wall clock across the two hosts. Phase order is what protects that: phase 1 is the headline and runs first, so an overrun in phase 2 costs the top-up rather than the result.

`jobs = 16` on phase 1 and 8 on phase 2. The 16 was sized by the `curves-calib` phase against 400 s runs, and a run holding its population for two hours has a different peak resident set.

## 13. Provenance

Branch `campaign/aurus-matched`, declared in `campaign.toml` beside this file, which carries the per-host seed split — av2 takes seeds 0 to 2, av3 takes 3 to 5 — so no range is ever chosen at a prompt. The split is over seeds and never over the factors, so both arms of every contrast run on one host and a host difference cancels inside the `(spec, seed)` cell.

Two config trees, `experiments/configs-matched` and `experiments/configs-long`, generated on the host at stage time by the two `gen_configs.py` calls the declaration joins with `&&`. The phases differ in more than a profile name, phase 1 fixing the budget in individuals and phase 2 fixing it in seconds, so one tree could not serve both.

On close, vendor `gen_configs.py`, `run_experiments.py`, `merge_experiments.py` and `score_curves.py` into `scripts/` beside this plan with their blob shas in `PROVENANCE.json`, and record whether the branch merged, was split or was rebased. `PROVENANCE.json` records the absence of a decision rule in the same field the other archives use for the decision taken, since a blank there would read as an unfinished close.

Matching a budget is the part of a tool comparison that is cheapest to get wrong and hardest to notice afterwards, the 400 s window of the previous campaign having looked fair until its generation counts were read. What this plan can promise is that the currency is stated before the rows exist, and that the archived AuRUS logs behind every figure above sit in a directory the analysis can be checked against.
