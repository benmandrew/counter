# `weighted` against `nsga2-apportion`: one key, 336 pairs

Pre-registered 2026-08-26, before any `selection-smoke` row existed. Section 7 records what this campaign will and will not license, which matters more here than usual: it is a smoke test and it pre-registers no decision. Section 8 names the hazards in advance so that none of them is discovered afterwards in the shape of whichever result arrives.

## 1. Question

`Config::selection_scheme` takes three values, and one of them has never been measured against the scheme counter ships. `WeightedAverage` (TOML `weighted`) ranks the population by the aggregate scalar fitness. The two *Non-dominated Sorting Genetic Algorithm II* (NSGA-II) schemes rank by non-dominated sorting and crowding distance over the individual objectives, and differ from each other only in the survivor step.

The campaign asks whether `weighted` still trades against NSGA-II the way the 2026-07-24 ablation measured it, on the engine counter ships on 2026-08-26 and at the fitness weights section 4 states rather than any counter defaults to. `selection_scheme` is the only config key that differs between the two arms.

## 2. Why the archived rows do not answer it

The last measurement of `weighted` is `experiments/2026-07-24-ablation`, run on 2026-07-24 and 2026-07-26 at gen10/pop200, whose `nsga2` arm is today's `nsga2-truncate`. Its *Temporal Logic Synthesis Format* (TLSF) half holds 3,076 rows, 1,548 nsga2 against 1,528 weighted, and reads as a trade in two directions at once: weighted found more repairs, `found_repair` 87.3% against 76.4% at an odds ratio of about 0.47, and NSGA-II implied more ideals, `implies_ideal` 23.7% against 14.9% at an odds ratio of about 1.78. Its FRETISH half, 1,280 rows, is a clean NSGA-II win on quality at 0.642 against 0.297, with both arms at 100% yield.

`weighted` has never been crossed against `nsga2-apportion`, which became the default on 2026-08-14 through campaign `2026-08-11-selection-default`. The two schemes appear together in no archived results CSV at all, the truncate/apportion split postdating weighted's retirement as the default.

The engine has also moved since that ablation, in six ways that all touch what the search reaches:

- the seven operator repairs of 2026-08-19, unconditional since 2026-08-20;
- the monotone rewrite arm and cloned assumptions of 2026-08-21;
- `Implies` in the TLSF binary grammar;
- the cross-generation accumulator, default on since 2026-08-25;
- MRS status grading with degree admission order;
- the weakening screen, defaulting off since 2026-08-20.

So the ablation's numbers describe a different engine. Its yield finding in particular is obsolete on this corpus, for the reason section 6 gives.

## 3. Design

Two arms, paired on `(spec, seed)`. Arm A is `nsga2-apportion`, the shipped default and the control; arm B is `weighted`, the treatment.

| | `selection-smoke` |
|---|---|
| corpus | 21 TLSF families (`SELECTION_SMOKE_SPECS`) |
| operating point | gen 10 / pop 200 |
| arms | `nsga2-apportion` (control), `weighted` (treatment) |
| seeds | 16, av2 0–7 and av3 8–15 |
| pairs | 21 × 16 = 336 |
| runs | 336 × 2 = 672 |
| jobs | 8 |

Both arms of a pair run on the same host at the same seed, so the seed split is over seeds and never over schemes, and a host difference cancels inside the pair. The operating point is the one the 2026-07-24 ablation ran, which is what makes the old rates readable beside the new ones.

The budget is 2 h per host, and 16 seeds is what fits it. The `monoon` arm of `2026-08-23-monotone` sums to 1,807 s of serial work per `(seed, arm)` over these 21 families, so 8 seeds of both arms is about 28,900 serial seconds per host — roughly 80 minutes at `jobs = 8` if the weighted arm costs what the control does, and inside 2 h at 1.5x. That estimate is the control arm's cost projected onto an arm with no archived timings, which is the same one-sided assumption the caps rest on, so it may be wrong in the same direction. The run order is seed-major — 42 runs per seed, both arms of every family — so a host stopped at the deadline leaves whole seeds and a balanced design rather than a corpus truncated mid-sweep. Overrunning therefore costs seeds, not the design.

## 4. Configuration, and the weights the campaign chooses

The configs are generated on the host at stage time by the `configs` line in `campaign.toml`. The two emitted TOML files were diffed and differ in exactly one line, `selection_scheme`. That is the campaign's whole claim about itself.

Every other key the two files state equals the built-in default in `include/config.hpp`, apart from the three aggregate fitness weights. The campaign states 0.1 / 0.2 / 0.7 — syntactic, semantic, status — through `gen_configs.py`'s new `--weights` flag, rather than inheriting the binary's 0.2 / 0.5 / 0.5 or the 0.33 triple the generator has pinned into every config it has ever written. The flag defaults to that pinned triple, so every past grid still generates byte-identically, which was verified.

A trace of the C++ places the choice inside the treatment arm alone. `nsga2_sort` (`include/genetic/nsga2.hpp:59-77`) builds its key from the per-objective vector, non-domination rank and crowding distance, and never reads the scalar. `order_population` (`include/genetic/pipeline.hpp:41-53`) compares the scalar only under `WeightedAverage`, and elitism takes the top `elite_n` from that same NSGA-II order (`include/genetic/pipeline.hpp:396-403`). The output path is content-based: `realizable_survivors` (`src/tlsf/survivors.cpp:118-140`) gates on realizability and the correctness table, and the weakening and implication filters match by specification value (`src/tlsf/survivors.cpp:166-181`). Every maximal spec is written whatever the scalar reads, which leaves the scalar fixing the order alone, and with it the `repair_N` numbering. The fitness cache keys on the specification and stores the objective vector (`include/fitness/function.hpp:57-90`), the scalar being computed on demand. Its remaining readers are cosmetic, being the status line, `best_fitness` in `run.json` and the dashboard, and `fitness.total` in the `repair_N.fitness.json` sidecar.

So the weights are a parameter of the treatment arm and of nothing else. The control arm is invariant to them: `implies_ideal`, `found_repair`, `n_repairs` and `wall_time_s` do not move with the triple there, and only the reported ordering and `best_fitness` do. A parameter applied unequally to two arms would be a confound because the control's behaviour depends on it, and this control's does not.

The choice is substantive. At 0.7 on status the weighted arm ranks chiefly by realizability and pays little for similarity, so it is measured in the configuration most favourable to yield and least favourable to `implies_ideal`. That sharpens the 2026-07-24 trade rather than sitting neutrally between its two directions.

Sweep T at level `monoon` is used because it is the only level in any TLSF sweep whose four keys — `p_monotone` 0.25, `p_clone_assumption` 0.25, `elitism_rate` 0.1 and `accumulate_repairs` true — all equal today's defaults, so the archived config states every key that has moved recently instead of inheriting it. `monoship` pins `elitism_rate` 0.0, a default move the 2026-08-23 monotone campaign measured and did not make.

## 5. Corpus, and what excluding four families costs

The corpus is `H2H_TLSF_READY` minus four families, excluded on measured cost alone: `humanoid-742` at a 7200 s mean and capped on every seed, `humanoid-531` at 6149 s, `pcar-v2-888` at 1643 s and `humanoid-503` at 685 s, all read off the `monoon` arm of `2026-08-23-monotone`. Together they are 84% of a seed's serial cost over the 25 families, and `humanoid-742` alone exceeds the whole 2 h per-host budget on one job.

The rule is cost and never outcome. A corpus chosen by how well the incumbent scheme scored on it would be selected on the response, and the premise of the comparison is that either arm may win a family.

The exclusion carries a cost that has to be stated. Three of the four sit at `implies_ideal` 0.10 under the control arm and one sits at 0.00, so the excluded set is nearly all headroom this campaign cannot see, and nothing here says how `weighted` does on the heavy families.

## 6. Endpoints, and where the headroom is

The primary endpoint is per-run `implies_ideal` greater than zero, paired by `(spec, seed)` and read by a two-sided exact *McNemar test* over the discordant pairs among the 336. Secondary and pre-registered as secondary: `found_repair`, `n_repairs`, and `wall_time_s` as a paired per-family *Wilcoxon signed-rank test*.

All four are weight-invariant on the control arm, by the trace in section 4. The move to 0.1 / 0.2 / 0.7 therefore changes one column there, `best_fitness`, and that column is not an endpoint.

`found_repair` reads 1.00 on all 21 families under the control arm in the monotone archive. The yield half of the 2026-07-24 trade therefore has no room to reproduce on this corpus: a weighted win on yield is not measurable here, and its absence must not be read as a reversal. That saturation is the reason `implies_ideal` is the primary.

Per-family `implies_ideal` under the control arm, from the `monoon` arm of `2026-08-23-monotone` at 20 seeds each, is what the campaign has headroom against.

| rate | families |
|---|---|
| 1.00 | `rg2`, `ltl2dba27`, `detector-aurus`, `lily02`, `simple-arbiter-aurus`, `ltl2dba-r-2`, `load-balancer-aurus`, `ltl2dba-theta-2` |
| 0.95 | `round-robin-arbiter-aurus` |
| 0.80 | `prioritized-arbiter-aurus` |
| 0.75 | `minepump` |
| 0.65 | `arbiter-aurus` |
| 0.60 | `full-arbiter-aurus` |
| 0.40 | `lily11` |
| 0.15 | `lily15` |
| 0.10 | `gyro-var2` |
| 0.00 | `lily16`, `rg1`, `gyro-var1`, `humanoid-458`, `lift` |

Eight families can only lose and five can only gain. The eight in between carry the power.

## 7. What the campaign licenses, and what it does not

No decision rule is pre-registered, because none would be honest at this budget. 336 pairs at a control rate near 0.5 on the eight informative families resolves roughly a 0.10 absolute difference, and anything smaller is out of reach here.

The campaign can say whether the 2026-07-24 quality gap, 23.7% against 14.9%, survives at anything like that size. It cannot settle the default. Settling that would need the four excluded families, the shipped fitness weights of 0.2 / 0.5 / 0.5, and the FRETISH path.

## 8. Threats to validity

**The result describes one setting of the fitness weights.** They are 0.1 / 0.2 / 0.7, for the reason section 4 gives, and the trace there rules out any bias in the contrast. What is left is a limit on generalisation. The weighted arm is measured at status-heavy weights alone, so nothing here speaks to `weighted` at the shipped 0.2 / 0.5 / 0.5 or at the 0.33 triple every archived campaign ran, and a loss on this campaign is no verdict on the scheme.

**Timeout caps are sized from the control arm alone.** They are 4× the per-family maximum from the `monoon` arm of `2026-08-23-monotone`, floored at 300 s and clamped at 3600 s, because that is the only arm with archived timings. If the weighted arm runs longer the caps bite it alone, which is one-sided censoring on the response under test, the failure `replicate-recap` exists to undo. Read `timed_out` per arm before reading `implies_ideal`; where the two arms' timeout counts differ, the primary endpoint is not readable as it stands.

**The corpus is a cost-selected subset.** Section 5 states the rule and what the four exclusions cost in headroom.

**TLSF only.** Most of the six engine changes in section 2 are TLSF-only, which is why, and the 2026-07-24 ablation measured both paths where this answers one of them.

**The archived `monoon` rows are a check, not a control.** They share this corpus, the seeds 0–15 overlap, the level and the scheme, so they are a free validity check on the fresh control arm. The weight change costs that check nothing, the control arm being weight-invariant by section 4's trace, which leaves the archived rows differing from the fresh control arm in two things: commit `6b78709` and a 7200 s cap. That cap censors differently from this campaign's, and both arms run fresh for that reason.

## 9. Deliberately not covered

Four things are out of scope by construction, and naming them here stops any of them being read into the result later.

- **The FRETISH path.** No FRETISH phase runs, so the 2026-07-24 FRETISH finding of 0.642 against 0.297 is neither replicated nor contradicted.
- **`nsga2-truncate`.** Only the shipped `nsga2-apportion` is crossed against `weighted`. The 2026-07-24 rows are the truncate arm, across a binary change, and no third arm runs here.
- **The four excluded families.** Section 5.
- **Any fitness weights but 0.1 / 0.2 / 0.7.** Section 4. Neither the shipped 0.2 / 0.5 / 0.5 nor the archived 0.33 triple is measured for the weighted arm.

## 10. Provenance

Branch `campaign/selection-smoke`, declared in `campaign.toml` beside this file. `scripts/campaign.py stage` puts each host on the branch, rebuilds it and runs the declaration's `configs` line, so the seed split and the generator invocation each exist in exactly one place and neither is chosen at the prompt. The profile is `selection-smoke` in `scripts/run_experiments.py`, registered in `merge_experiments.PROFILE_CSVS`.

On close, vendor `gen_configs.py`, `run_experiments.py` and `merge_experiments.py` into `scripts/` beside this plan with their blob shas in `PROVENANCE.json`, and record whether the branch merged or was split. Attribution is `recorded` where both hosts were staged through `stage`, the binary's commit having been verified there before launch.

The strongest reading this design supports is a comparison of two selection schemes at one operating point, on a corpus chosen for what it costs and at fitness weights counter does not ship. Writing the weights and the exclusions down before the rates exist is what keeps a null from being mistaken for a verdict on the default.
