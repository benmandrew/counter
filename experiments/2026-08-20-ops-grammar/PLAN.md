# The repaired mutation and crossover grammar: `opslegacy` against `opsfixed`

Pre-registered 2026-08-20, before any `ops-grammar` row was collected. The `ops-pilot` rows predate it and carry no endpoint: they are wall-clock measurements taken at a non-campaign cap, and section 6 is the only thing that reads them. Section 9 registers a binding accept/reject rule.

## 1. Question

The audit in `experiments/2026-08-14-aurus-h2h/REPORT.md` found seven defects in counter's *mutation and crossover grammar*, the set of edits the genetic search can apply to a candidate specification. All seven were repaired on 2026-08-19. Every campaign archived before that date ran the unrepaired grammar.

The campaign asks whether the seven repairs improve repair quality over that grammar, at the operating point counter would ship.

## 2. Why the question is open

The evidence so far is two things, and neither decides the question.

The first is a static audit of 2,295 archived repairs. It shows that certain moves were unreachable under the old grammar. It does not show that reaching them helps.

The second is a local smoke test over 20 seeds and 4 TLSF families, with unpaired arms and no pre-registered rule: `minepump` went 1/20 to 3/20, `arbiter-aurus` 10/20 to 15/20, `rg2` 20/20 to 20/20 and `lily02` 19/20 to 20/20. The control was the pre-change binary on the same box, which reproduced the archived campaign rates exactly.

A regression is a live possibility rather than a formality. The graft grows formulae, which costs bloat-cap headroom and synthesis time. An eighth recommended fix from the same report, a per-section bloat baseline, was implemented and measured: it cost `arbiter-aurus` 9 of its 10 ideal-implying runs and `rg2` 2 of 20, and reverting it alone recovered 8 of the 9. This family of changes has already produced one regression under measurement.

## 3. Design

One binary serves both arms. `[genetic] repaired_operators` is a boolean defaulting to false, and it gates all seven repairs. Two binaries were impossible here, because `campaign.py` stages one branch per campaign and `commit` may never join `KEY_FIELDS`, so the two arms would collide on the merge key.

The factor is sweep O at two levels, `opslegacy` (control) and `opsfixed`, paired on `(spec, seed)`.

| | `ops-fret` | `ops-tlsf` |
|---|---|---|
| corpus | 4 FRETISH families | the 12 `H2H_TLSF_SPECS` families |
| operating point | gen 40 / pop 1000 | gen 10 / pop 200 |
| arms | `opslegacy`, `opsfixed` | `opslegacy`, `opsfixed` |
| selection scheme | `nsga2-truncate` | `nsga2-truncate` |
| seeds | 30 (av2 0–14, av3 15–29) | 20 (av2 0–9, av3 10–19) |
| runs | 4 × 2 × 30 = 240 | 12 × 2 × 20 = 480 |
| jobs | 4 | 4 |

The FRETISH operating point is the one `cj-large`, `metric` and `ablate-fret` ran. Both arms of a pair run on the same host at the same seed, so a host difference cancels inside the pair.

`accumulate_repairs` is true on both arms and is not crossed. Sweep N measured it alone in `2026-08-19-accumulator`, at McNemar 6-0, p = 0.0312, with a median paired wall ratio of 1.034, and it is a candidate default. Holding it on asks what the grammar adds beyond what accumulation already recovers, which is the question that decides whether to ship.

Both paths run. Three of the seven repairs are FRETISH-side (atom graft, atom-rename distinctness, uniform graft site) and four are TLSF-side (per-section pools, fourth temporal arm, assumption template, single-occurrence graft). A TLSF-only campaign would ship three repairs unmeasured.

The TLSF configs set the `ltlsynt` budget to 10000 ms, not the 500 ms the 2026-08-14 head-to-head inherited from `ablate-tlsf`. A tight synthesis budget censors whichever arm builds larger formulae, and that is the treatment arm by construction. Censoring the treatment fakes a null. The consequence is that the archived counter rows are not a control for either arm, so the campaign carries its own control, which is what the pairing is for. The archived AuRUS rows remain a reference for both arms, since counter's internal synthesis budget is a configuration of counter rather than a term of the shared 7200 s wall cap. AuRUS is not re-run.

## 4. What the campaign does not control

Three simplifier folds landed with the repairs and are not behind the key: `G G φ → G φ`, `F F φ → F φ`, and the U/W/R self-join folds in `src/prop_formula/simplify.cpp`. They are semantics-preserving and `Formula::simplify()` has no config access, so they ship in both arms. The campaign therefore measures the grammar repairs *given* the folds, and whatever the folds contributed to the smoke test is invisible here.

This is not a formality. `test_weakening_screen_rejects_non_weakening` fails with the key off, because the legacy grammar plus the new folds is ground neither arm was ever measured on, and under it the search reaches a genuine weakening where main reaches a non-weakening; restoring main's `simplify.cpp` makes it pass. The folds change what the search finds, not only how large its formulae are. The control arm is therefore not main's behaviour, but the behaviour counter will ship if the key stays false, which is the right control for the shipping decision and the wrong one for attributing anything to the folds.

The reading of the result follows from that. The smoke test compared folds plus repairs against a binary with neither, the pre-change binary at `d37ce0e`. Sweep O compares repairs against no repairs with the folds on both sides. It measures less, so an effect smaller than the smoke test's 10/20 to 15/20 on `arbiter-aurus` is the expectation rather than a contradiction.

## 5. Endpoint

The primary endpoint is the paired per-run `implies_ideal`, tested by exact McNemar over the discordant pairs and computed separately per path. This follows the accumulator campaign's precedent.

Reported but not decisive: `found_repair` yield, `n_repairs`, the `best_relation` distribution, the median paired wall ratio, and the `timed_out` rate per arm.

`n_repairs` is explicitly not a headline. It counts the maximal antichain left by the implication filter, so one dominating repair collapses several into itself and the count can fall while quality rises.

## 6. Cost model

The archived figures come from `2026-07-24-ablation`, default cell: TLSF cost 552 s of counter per seed summed over the 12 families, with `humanoid-531` dominating at a 377 s median and already cap-bound at 600 s, so its true tail was never observed; FRETISH cost 100 s per seed over its 4 families.

That model is stale in four ways at once, all of them upward. MRS status grading became the default on 2026-08-11 at 1.89x cost, `p_remove_guarantee` landed, the accumulator is on in both arms, and the `ltlsynt` budget moves from 500 ms to 10000 ms. Caps are therefore pilot-sized at 4x the slowest run of a pilot over this corpus and operating point with both arms measured, floored at 600 s.

The pilot is campaign `ops-pilot`, a separate declaration on the same branch: the three slowest families `gyro-var1`, `lift` and `humanoid-531` crossed with both arms over 4 seeds, at `jobs = 4` to match the phase it sizes, under a deliberately non-binding 3600 s cap. Its rows keep their own CSV and never merge into this campaign's, because a timing measurement taken at a non-campaign cap is not an endpoint observation.

The TLSF phase runs at `jobs = 4` rather than the `jobs = 1` every other TLSF profile declares, and the pilot matches it. That convention guards RAM, ltlsynt being multi-GB per call against a per-process concurrency cap, and the guard binds on a 30 GB box rather than on av2 and av3 at 125 GB each; the largest tool process observed while piloting was a 9.8 GB `ltl2tgba`, so four concurrent runs peak near 40 GB against 114 GB available. What `jobs = 1` cost was cores: a run there already gets `parallel = 32` and host load still sat near 4, counter being blocked on child processes rather than CPU-bound, which is the same thing the profiler reports as a `proc/read` cpu/wall ratio near 0.01. The runner sets `parallel_k = cpu_count // jobs`, so the thread budget is constant and neither setting oversubscribes.

The concurrency is why the pilot has to run at the campaign's own `jobs`. Each run gets a quarter of the threads, so per-run wall time rises even as throughput improves, and a cap sized at `jobs = 1` would censor — directionally against the treatment arm, which is the failure mode section 8 registers. A first pass of the pilot ran at `jobs = 1` and produced six rows; they were deleted rather than carried forward, because they size a campaign that is no longer the one being run, and because mixing two concurrencies in one cap-sizing dataset would hide exactly the effect the caps exist to bound.

The pilot ran to completion on 2026-08-20, 24 of 24 rows, none of them timed out under the non-binding cap, so every figure below is an observed maximum rather than a censored bound.

| spec | `opslegacy` median / max | `opsfixed` median / max | cap |
|---|---|---|---|
| `gyro-var1` | 59.0 / 94.4 s | 64.8 / 118.5 s | 600 s (floor) |
| `lift` | 129.0 / 209.7 s | 145.6 / 236.1 s | 960 s |
| `humanoid-531` | 310.6 / 390.4 s | 456.5 / 630.4 s | 2580 s |

The nine unpiloted families keep the 600 s floor on archive evidence: the slowest of them is `gyro-var2` at an 85.9 s archived maximum, against `gyro-var1`'s archived 86.7 s, which piloted at 118.5 s, leaving the floor at roughly 5x the expected maximum.

`humanoid-531` is the family the pilot existed for, and it justified the exercise. Its treatment arm ran to 630.4 s against the control's 390.4 s, so the 600 s cap this campaign would otherwise have inherited from the head-to-head fires on `opsfixed` and never on `opslegacy`. That is precisely the directional censoring section 8 registers, it would have surfaced as the treatment losing, and no amount of care in the analysis would have recovered it from the rows.

One figure is registered here because it is uncomfortable rather than because it is reassuring. The paired wall ratio over the pilot's 12 pairs has a median of **1.250**, sitting exactly on the bound that decision-rule outcome 1 imposes. The bound was fixed a priori, following the accumulator campaign, and it does not move now: adjusting a threshold after seeing a measurement land on it is the failure this document exists to prevent. Two things qualify the number without softening it. The pilot covers the three most expensive families and none of the nine cheap ones, and the spread is enormous — per-family medians of 0.733, 1.070 and 1.598, with individual pairs from 0.241 to 5.828. The campaign's own 240 pairs are what the rule reads, not these 12.

The campaign is 480 TLSF runs plus 240 FRETISH runs, 720 in total, split evenly over av2 and av3.

## 7. Power, and what a null means

The design gives 240 TLSF pairs and 120 FRETISH pairs. The accumulator campaign rejected at 100 pairs with 6 discordant pairs all one way (p = 0.0312), and exact McNemar needs 6 one-way discordant pairs to reach p < 0.05 regardless of n. If the smoke test's `arbiter-aurus` effect is real and confined to that family, it contributes about 5 discordant pairs on its own.

A null bounds the effect below what 240 pairs resolve. It is not evidence of no effect.

## 8. Threats to validity

Recorded before the fact, so none of them is discovered afterwards in the shape of whichever result arrives.

**Cap censoring, directional against the treatment.** The treatment arm builds larger formulae, so a cap that fires fires on it first. The mitigation is the pilot-sized caps of section 6; `timed_out` per arm is the validity check, and if the two arms' rates differ materially the primary endpoint is read with that caveat and `humanoid-531` is analysed separately.

**Frozen ideals.** Incomparable families bias `implies_ideal`. Both arms score against the same ideals, so the bias is common-mode and cancels in the pairing.

**The simplifier folds are uncontrolled.** Section 4 states what that costs the attribution.

**Cost model vintage.** Section 5 states the four ways the archived figures are stale, and the pilot exists to replace them.

**The weakening screen is held on, and is about to stop being the default.** Added 2026-08-20 after the design was fixed; it changes nothing in section 9 and is recorded as a limit on how far the result carries. Both arms run `run_weakening = true`, which is `include/config.hpp`'s default today and what every archived campaign ran. That default is going to false. Because the screen is common to both arms it cancels inside each pair, so the endpoint stays valid for the configuration measured — but it caps both arms, and the cap is not small: the `fsm-combined` investigation found 10 of 12 ideal-implying candidates dying in `final/weakening`, and the 2026-08-19-weakening-arbiter campaign measured the screen costing 9 of 120 paired repairs at p = 0.002. Removing it therefore raises headroom on both arms, and plausibly not symmetrically, the treatment arm having produced 76% more gate-passing candidates for the screen to discard. This campaign's result must not be read as predicting the comparison under the new default; that is a separate replication, and it is the one that decides what ships.

**Host differences.** Both arms of a pair run on the same host, so a host difference cancels inside the pair.

## 9. Decision rule

Pre-registered and binding. Four outcomes, and the campaign takes exactly one of them.

1. The repaired arm wins the primary endpoint on at least one path at p < 0.05, loses neither path significantly, and the median paired wall ratio is under 1.25: flip the default to true and add `repaired_operators` to the "Config vintage" note in `experiments/README.md`, since every archived config omits the key and a moved default silently changes what each of them means. The key stays. Retiring it would reject this campaign's own configs, which state it on both arms, the way removing `[filters.intervals]` retired the `wellsep-timing` profile and TLSF sweep V along with it.
2. The repaired arm wins one path and loses the other: do not flip. Keep the key, and report per path.
3. No significant difference either way: do not flip. The repairs stay behind the key, and `REPORT.md`'s claim is downgraded to "the moves are reachable now, and reaching them did not measurably help at this corpus and budget".
4. The repaired arm loses: revert. The seven landed as one commit, so an ablation is a fresh campaign rather than a re-read of this one's rows.

## 9a. Amendment, 2026-08-20: the wall-ratio bound is per path

Outcome 1 reads "wins the primary endpoint on at least one path at p < 0.05, loses neither path significantly, and the median paired wall ratio is under 1.25". It does not say whether that ratio is the winning path's or a figure pooled across both, and the two readings diverge far enough to change the decision. The ratio is hereby **per path, gating the path that won**: a path triggers outcome 1 only if it both wins its own endpoint and holds its own median paired wall ratio under 1.25. A rule whose win criterion is per path cannot coherently carry a global cost gate, and nothing else in section 9 pools the paths.

**What was known when this was written, stated plainly because it is the only thing that makes the amendment worth anything.** The FRETISH path had completed and been scored: 120 pairs, 9 treatment-only wins against 3, exact McNemar p = 0.146, and a median paired wall ratio of 1.699 with a minimum of 1.193. So the FRETISH ratio was already known to fail the bound under either reading, and FRETISH already failed the endpoint conjunct independently — this amendment cannot rescue it and does not try to. The TLSF path stood at 10 of 240 rows on av2 and 32 of 240 on av3, so the figure the amendment actually governs did not exist and could not be anticipated from what did: the pilot's TLSF families gave a median ratio of 1.250 against FRETISH's 1.699, and the two operating points differ by a factor of four in generations and five in population.

The alternative was to leave the ambiguity standing and resolve it after the TLSF rows landed, which is worth nothing at all. Recording it here, against a timestamp that precedes the data, is what makes it checkable. Outcome 1 is now harder for FRETISH than a pooled reading would have made it and unchanged for TLSF, which is the direction an amendment should run when the amender has already seen half the results.

## 10. Provenance

Branch `fix/mutation-operators`. The campaign rests on four commits: `d46351f` "docs(experiments): document mutation/crossover audit from aurus-h2h", `3e9fb62` "fix(genetic): repair mutation and crossover grammar defects from aurus-h2h", `6a4466d` "feat(config): gate the operator repairs behind `repaired_operators`", and `aa775c7` "feat(experiments): add sweep O and the ops-grammar campaign".

Sweep O lives in `scripts/gen_configs.py`, the profiles `ops-tlsf` and `ops-fret` in `scripts/run_experiments.py`, and both profiles are registered in `merge_experiments.PROFILE_CSVS`. The declaration is `experiments/ops-grammar/campaign.toml`, which carries the branch, the per-host seed ranges and the phases, so the seed split exists in one place and is never chosen at the prompt. AuRUS is not re-run, and its 2026-08-14 rows stand as a reference for both arms.

Both hosts are staged from the branch head before launch, and `campaign.py stage` verifies that each binary reports the declared commit with `dirty=0`, so attribution is `recorded` rather than inferred when the campaign closes.

The strongest reading this design supports is a comparison between two shipping configurations of one binary, which is narrower than a claim about the seven repairs themselves. Writing the folds down as uncontrolled before the run is what keeps that distinction available afterwards.
