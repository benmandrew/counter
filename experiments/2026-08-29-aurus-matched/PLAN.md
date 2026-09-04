# counter against AuRUS on a matched search budget

Pre-registered 2026-08-29, before any row of this campaign existed. Section 2 is the one a reader arriving from another plan in this archive needs first: this campaign registers no decision rule and no primary endpoint, and everything after section 2 is written on that footing.

## 1. Question

counter's search budget is a count of `generations` and AuRUS's is a count of individuals, so the two tools have never been stopped by the same rule. The `2026-08-14-aurus-h2h` campaign gave counter 10 generations at population 200 against AuRUS's own stopping rule, which is roughly twice the individuals; the `2026-08-28-selection-grading` campaign gave counter a 400 s deadline against nothing at all. Neither says which tool finds more repair per unit of search.

Three keys landed on 2026-08-29 to close that gap — `[genetic] termination`, `max_individuals` and `max_wall_s` — and this campaign is the first use of them. It fixes the budget in AuRUS's currency and lets wall time vary, so wall time becomes the measured outcome rather than the budget. One phase, 3000 runs, 30 seeds a family.

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

## 4. The individuals-matched cross, profile `matched`

The 2x2 of the 2026-08-28 campaign, run again in AuRUS's currency. `genetic.selection_scheme` crosses `fitness.status_grading` over the 25 families of `H2H_TLSF_READY` at seeds 0 to 29. Four arms, 3000 runs.

| arm | `selection_scheme` | `status_grading` |
|---|---|---|
| 1 | `nsga2-apportion` | `mrs` |
| 2 | `nsga2-apportion` | `aurus` |
| 3 | `weighted` | `mrs` |
| 4 | `weighted` | `aurus` |

Thirty seeds a family matches the 30 repeats AuRUS's archived corpus carries, 15 per host across av2 and av3, so both discovery curves are estimated off the same number of runs. The endpoint is a survival curve, which 6 repeats per family per arm estimates poorly.

Three flags carry the match. `genetic.termination = "individuals"` and `max_individuals = 1000` are AuRUS's rule; `population_size = 100` is `GA_POPULATION_SIZE` from the same banner. The population matters as much as the cap, since 1000 individuals at population 200 is half the generations of 1000 at population 100.

`generations = 500` is a ceiling that must not bind. counter breeds `population_size - elite_n = 90` offspring a generation at `elitism_rate = 0.1`, and counts only those differing from their parent, so 1000 individuals needs at least 12 generations. The external `timeout_caps` sit at 7200 s, matching AuRUS's `GA_EXECUTION_TIMEOUT`, and should not bind either.

The rest of the configuration is the shipping one and is fixed across all four arms: `runtime.parallel = 1`, `filters.run_implication = true`, `genetic.accumulate_repairs = true`, `[tlsf] repair_mode = "monolithic"`, the log similarity metric, and the weakening screen off. Single-threaded runs match AuRUS and keep the endpoint off the host, wall time being what is measured.

## 5. Why the 2026-08-28 campaign cannot answer this

That campaign gave every arm a 400 s `genetic.max_wall_s`. Its 355 surviving manifests read a median 81 generations run at population 200, with p90 256 and a maximum of 461 against a 500-generation ceiling. At 180 offspring a generation that median is about 16,200 bred individuals, roughly 16x AuRUS's budget.

A 400 s window therefore hands counter an order of magnitude more search than AuRUS is allowed. Any claim that counter found more inside 400 s is a claim about the budget rather than about the search, and is not defensible as written. This campaign removes that asymmetry by fixing the budget and measuring the time.

## 6. Why there is no second phase

A second phase was declared and dropped before launch. It would have given counter AuRUS's 7200 s deadline at population 200 on the six families where AuRUS's own first solution lands past 400 s: `humanoid-503`, `prioritized-arbiter-aurus`, `full-arbiter-aurus`, `humanoid-531`, `humanoid-458` and `pcar-v2-888`. Its premise was that a 400 s horizon truncates counter where it truncates AuRUS. The 2026-08-28 campaign's own 600 collected rows falsify that premise, read out of `experiments/results-curves.csv` at 24 runs a family, a 400 s search deadline and a 3600 s external cap.

| family | n | mean_s | max_s | timed out | found repair | implies ideal |
|---|---|---|---|---|---|---|
| humanoid-503 | 24 | 494 | 612 | 0 | 15 | 0 |
| prioritized-arbiter-aurus | 24 | 1877 | 3600 | 9 | 14 | 10 |
| full-arbiter-aurus | 24 | 1565 | 3600 | 6 | 17 | 11 |
| humanoid-531 | 24 | 3600 | 3600 | 24 | 0 | 0 |
| humanoid-458 | 24 | 968 | 2260 | 0 | 24 | 0 |
| pcar-v2-888 | 24 | 680 | 1195 | 0 | 24 | 0 |
| (all 25 families) | 600 | 2012 | 3600 | 245 | 340 | 184 |

counter already repairs five of the six inside a 400 s search. `humanoid-503` is the sharp case in reverse: AuRUS needs a median 2425 s to its first solution there, and counter finds a repair on 15 of 24 runs inside 400 s. `humanoid-531` is the one unrepaired family, and more search time is the wrong medicine for it, since all 24 of its runs died at the external cap with mean equal to max. What fails there is the post-search gate rather than the search.

The phase would have cost about 27 h of wall clock giving counter more of the one resource it already holds in surplus, which is the error the individuals budget exists to correct. That time went into seeds instead, 30 a family rather than 6. The `matched-long` profile and its spec list are deleted from `run_experiments.py` outright rather than left standing unused.

## 7. One phase reproduces AuRUS's termination criterion

Dropping the second phase costs nothing in the AuRUS comparison. AuRUS stops on whichever comes first of 1000 individuals or 7200 seconds, and this campaign gives counter the same pair: `max_individuals = 1000`, no `genetic.max_wall_s` at all, and the profile's 7200 s external cap. That is AuRUS's termination criterion reproduced rather than approximated.

A row whose manifest reads `stopped_by = individuals` spent its entire budget with the deadline slack, so a 7200 s deadline would have changed nothing about it. `stopped_by` is a per-row certificate rather than a counterfactual argument. Rows killed at the external cap are reported as censored, exactly as AuRUS's own 173 killed runs are, and `humanoid-531` is the family to expect there.

What cannot be claimed is anything about counter's behaviour beyond 1000 individuals. There is no AuRUS number to compare such a claim against, AuRUS never running past that either.

## 8. The pre-flight run

The mode was run once at this configuration before launch, on av2's existing binary, over `minepump` at seed 0 and single-threaded.

| field | value |
|---|---|
| `stopped_by` | `individuals` |
| `generations_run` | 26, of a 500 ceiling |
| `individuals_bred` | 1000, exact, with no overshoot |
| `wall_s` | 210.3 |
| `n_repairs` | 33, from 134 realizable |
| `accumulated/index.tsv` rows | 134, last accumulation at 73.1 s |

Every verification check registered in section 12 passes on it. The tail dominates. Search accounts for 73.1 s of the run and the post-search gate and implication filter for the remaining 137 s, which is 65% of the wall time at this budget.

## 9. What the paper does not fix, and what must not be claimed against it

Three gaps between the paper and the code are recorded here rather than discovered during analysis.

**The weights.** The paper does not fix one configuration. It names (α=.7, β=.1, γ=.2) among three that "typically reach better performance", and reports its Table 2 "with the best-performing configuration for each case study" — per case, not once globally. The archived AuRUS runs used one global setting, as counter does here through `--weights 0.1 0.2 0.7`, so the head-to-head is symmetric between the two tools. Neither side reproduces the paper's table, and no claim in `REPORT.md` may be made against it.

**The status ladder.** The paper describes a five-level ladder (1, 0.5, 0.2, 0.1, 0). `ModelCountingSpecificationFitness.java` has six, inserting 0.05 for `SPEC_STATUS.GUARANTEES`. counter's `fitness.status_grading = "aurus"` reproduces the code's six, so it matches what the archived runs did and diverges from the published description by one level.

**The individual count.** The paper is silent on how an individual generated is counted. It does not say the counter increments once per mutant and once per crossover offspring, nor that an offspring equal to its parent is excluded. counter's `SearchBudget` matches the code on both, so the budget match is a code-level match and has to be described as one.

## 10. AuRUS's timeout does not fire

Of the 780 archived AuRUS runs, 173 (22.2%) were killed by the harness at 7500 s, concentrated in six hard families. The internal `GA_EXECUTION_TIMEOUT = 7200` printed its `GENETIC ALGORITHM TIMEOUT REACHED` message in 0 of those 780 logs.

The reason is structural. `checkTermination()` sits between offspring in the breeding loops and cannot interrupt the `parallelStream()` fitness evaluation of the whole population, so a run stuck in scoring overruns its own deadline and is killed from outside. The paper's Table 2 reports 7400 s for five cases against its stated 7200 s cap, which is the same behaviour surfacing in the published numbers.

counter's `max_wall_s` has the matching limit and it is recorded in the root `CLAUDE.md`: nothing interrupts filters, scoring or the final gate, so a run overruns by whatever the generation it was in had left. Both tools therefore overshoot their internal deadline, for the same reason, and the external cap is what bounds each.

## 11. The six metrics

All six come from `<run-dir>/accumulated/index.tsv` through `scripts/score_curves.py`, which reads the accumulator's flushed record of every candidate that passed the output gate and the elapsed time it was found at. Four are curves against elapsed time and two are scalars.

- **`solutions`** — gate-passing candidates found by time t.
- **`ideal_solutions`** — of those, the ones equivalent to or strictly stronger than an ideal under `compare`.
- **`time_to_first_repair`** — elapsed seconds to the first gate-passing candidate.
- **`time_to_first_ideal_repair`** — elapsed seconds to the first ideal-implying one.
- **`maximal_solutions`** — the maximal antichain of the set found by time t.
- **`maximal_ideal_solutions`** — of those survivors, the ideal-implying ones.

The last two are computed offline under `--maximality`, after and apart from the timed run, at a bounded number of log-spaced time cuts. `maximal` is a pairwise implication sweep that has taken 19 GB on this corpus, so running it inside a timed run would put its cost into the measurement it describes. Their cost never enters a measured time.

Censoring follows the 2026-08-28 campaign unchanged. A run that never finds an ideal-implying repair has no time to one, and `score_curves.py` writes every such value as an empty field with an explicit `censored` flag rather than a zero or an omitted row.

## 12. Verification checks

Five checks come before anything is drawn from these rows. Each is cheap, and each catches a way the campaign silently measures something other than what it declares.

1. **`stopped_by` must read `individuals` on essentially every row.** A row reading `generations` means the 500-generation ceiling bound instead of the individuals cap, and that row measures a different budget. `stopped_by`, `generations_run` and `individuals_bred` are in the manifest at schema 21 and are the fields to read.
2. **`individuals_bred` should sit at or just above 1000.** The budget is checked between offspring slots inside `breed_offspring`, so a small overshoot within one slot is expected; the pre-flight read exactly 1000. A large one is a bug in the budget wiring.
3. **A flat `ideal_solutions` at zero across every arm points at the join first.** `score_curves.py` joins `compare`'s per-repair output back to the accumulation index by file name, and a broken join reads as a search that found nothing ideal-implying anywhere. Cross-check against the `implies_ideal` column of the results CSV, which comes from a separate `compare` call per run.
4. **Read this campaign's cost off its own first ~40 rows rather than predicting it.** The 2026-08-28 campaign's estimate was low by a factor of 2.2, because `genetic.max_wall_s` bounds the generation loop and nothing after it — the final realizability gate, the weakening screen and the implication filter all run past the deadline uncapped. This campaign sets no `max_wall_s`, so it does not carry that error in that form, and the pre-flight's 65% tail is one run rather than a distribution. Forty rows give a mean to hold against the 500 s section 13 assumes.
5. **Count the censored runs before any analysis.** A censored run is a run directory holding `accumulated/index.tsv` with no `run.json`. In the 2026-08-28 campaign 245 of 600 runs were censored at a 3600 s cap, which is why the cap here is 7200 s.

## 13. Cost

At a mean of 500 s a run the 3000 runs come to about 13 h of wall clock over the two hosts at `jobs = 16` each. The available budget is roughly 68 h of wall clock across av2 and av3.

The 2012 s mean of the 2026-08-28 campaign is the pessimistic bound on that figure. Its search was 400 s against this campaign's 73.1 s, and the post-search gate and implication filter walk an accumulated set sized by the search that produced it, so a budget of 1000 individuals leaves far less to filter.

`jobs = 16` comes from the declaration and was sized by the `curves-calib` phase against 400 s runs.

## 14. Provenance

Branch `campaign/aurus-matched`, declared in `campaign.toml` beside this file, which carries the per-host seed split — av2 takes seeds 0 to 14, av3 takes 15 to 29 — so no range is ever chosen at a prompt. The split is over seeds and never over the factors, so both arms of every contrast run on one host and a host difference cancels inside the `(spec, seed)` cell. It must stay equal to the `matched` profile's seed list, the two being the same number written twice.

One config tree, `experiments/configs-matched`, generated on the host at stage time by the `gen_configs.py` call the declaration carries.

On close, vendor `gen_configs.py`, `run_experiments.py`, `merge_experiments.py` and `score_curves.py` into `scripts/` beside this plan with their blob shas in `PROVENANCE.json`, and record whether the branch merged, was split or was rebased. `PROVENANCE.json` records the absence of a decision rule in the same field the other archives use for the decision taken, since a blank there would read as an unfinished close.

Matching a budget is the part of a tool comparison that is cheapest to get wrong and hardest to notice afterwards, the 400 s window of the previous campaign having looked fair until its generation counts were read. What this plan can promise is that the currency is stated before the rows exist, and that the archived AuRUS logs behind every figure above sit in a directory the analysis can be checked against.
