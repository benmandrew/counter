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
| jobs | 4 | 1 |

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

The pilot is campaign `ops-pilot`, a separate declaration on the same branch: the three slowest families `gyro-var1`, `lift` and `humanoid-531`, cheapest first, crossed with both arms over 4 seeds, at `jobs = 1` to match the phase it sizes, under a deliberately non-binding 3600 s cap. Its rows keep their own CSV and never merge into this campaign's, because a timing measurement taken at a non-campaign cap is not an endpoint observation.

[PILOT NUMBERS PENDING]

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

**Host differences.** Both arms of a pair run on the same host, so a host difference cancels inside the pair.

## 9. Decision rule

Pre-registered and binding. Four outcomes, and the campaign takes exactly one of them.

1. The repaired arm wins the primary endpoint on at least one path at p < 0.05, loses neither path significantly, and the median paired wall ratio is under 1.25: flip the default to true and add `repaired_operators` to the "Config vintage" note in `experiments/README.md`, since every archived config omits the key and a moved default silently changes what each of them means. The key stays. Retiring it would reject this campaign's own configs, which state it on both arms, the way removing `[filters.intervals]` retired the `wellsep-timing` profile and TLSF sweep V along with it.
2. The repaired arm wins one path and loses the other: do not flip. Keep the key, and report per path.
3. No significant difference either way: do not flip. The repairs stay behind the key, and `REPORT.md`'s claim is downgraded to "the moves are reachable now, and reaching them did not measurably help at this corpus and budget".
4. The repaired arm loses: revert. The seven landed as one commit, so an ablation is a fresh campaign rather than a re-read of this one's rows.

## 10. Provenance

Branch `fix/mutation-operators`. The campaign rests on four commits: `d46351f` "docs(experiments): document mutation/crossover audit from aurus-h2h", `3e9fb62` "fix(genetic): repair mutation and crossover grammar defects from aurus-h2h", `6a4466d` "feat(config): gate the operator repairs behind `repaired_operators`", and `aa775c7` "feat(experiments): add sweep O and the ops-grammar campaign".

Sweep O lives in `scripts/gen_configs.py`, the profiles `ops-tlsf` and `ops-fret` in `scripts/run_experiments.py`, and both profiles are registered in `merge_experiments.PROFILE_CSVS`. The declaration is `experiments/ops-grammar/campaign.toml`, which carries the branch, the per-host seed ranges and the phases, so the seed split exists in one place and is never chosen at the prompt. AuRUS is not re-run, and its 2026-08-14 rows stand as a reference for both arms.

Both hosts are staged from the branch head before launch, and `campaign.py stage` verifies that each binary reports the declared commit with `dirty=0`, so attribution is `recorded` rather than inferred when the campaign closes.

The strongest reading this design supports is a comparison between two shipping configurations of one binary, which is narrower than a claim about the seven repairs themselves. Writing the folds down as uncontrolled before the run is what keeps that distinction available afterwards.
