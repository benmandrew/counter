# Weakening restriction on the arbiter families: `wkon` against `wkoff`

Pre-registered 2026-08-19, before any row at the campaign commit was collected. No accept/reject rule is registered; §7 sets out why.

## 1. Question

Counter emits only *weakenings* of the input specification — repairs `R` where `original ⟹ R`. The restriction is enforced by `run_weakening_filter`, a final screen at step 6 of the pipeline, and `run_weakening` defaults to true (`include/config.hpp:111-118`).

That forecloses *incomparable* repairs: ones that both add and remove behaviours. Incomparability is the shape of a substitution error, where the engineer wrote signal `req` and meant `ack`, or wrote `after 3 ticks` and meant `within 3 ticks`. Neither correction is a weakening of what was written, so neither can leave the pipeline.

The campaign asks whether the restriction costs reachable repairs on the arbiter families, which currently yield close to nothing.

## 2. Why the question is open

An audit of the pipeline found the restriction enforced in exactly one place, and the search reaching past it everywhere else.

The mutation operators already produce incomparable candidates freely. `mutate_atom_name` (`src/genetic/mutation.cpp:40-53`) draws a replacement atom uniformly from the declared alphabet, so a guarantee over `req` becomes a guarantee over `ack` with no reference to what the original said. `strengthen_within_timing` (`:198`) converts `within n` into `after n-1`, which is a different obligation rather than a weaker one. The code says so at `mutation.cpp:369-371`.

Nothing downstream of the operators re-imposes the restriction. The per-generation filter chain never consults the original's semantics. The maximality filter keeps antichains rather than chains, so two incomparable repairs both survive it (`compute_subsumed`, `src/filter/implication.cpp:43-47`). The realizability gate never references the original at all (`src/tlsf/survivors.cpp:43`). `run_weakening_filter` is the sole rejection point.

The archive cannot answer the question either. Of 431k archived result rows, zero carry `weakening = wkoff` for any arbiter-family specification. The FRETISH weakening ablation did run, on `cj-large`, over 9,796 paired runs: the filter lost 1,005 pairs and won 410. The TLSF arbiter families have never been run with it off.

## 3. Design

One factor, `weakening`, at two levels, crossed over the arbiter corpus. The two emitted configs differ in exactly one line, `run_weakening`.

| | `weakening-arbiter` |
|---|---|
| corpus | the 10 `ARBITER_PROBE_SPECS` families |
| operating point | gen 10 / pop 200 |
| arms | `wkon`, `wkoff` |
| selection scheme | `nsga2-truncate` |
| seeds | 20 (av2 0–9, av3 10–19) |
| runs | 10 × 2 × 20 = 400 |
| jobs | 1 |
| per-run cap | 900 s flat |

The corpus is the one `2026-08-10-arbiter-probe` ran: `arbiter`, `arbiter-aurus`, `arbiter-handshake`, `full-arbiter`, `prioritized-arbiter`, `round-robin-arbiter`, `simple-arbiter`, `amba`, `load-balancer` and `lily02`. Reusing it keeps the cost model in §5 measured on the same specifications at the same operating point.

Generations, population size and selection scheme sit at the shipped defaults, so the baseline arm samples the configuration a user gets.

Configs come from one generator invocation.

```sh
python scripts/gen_configs.py --tlsf --weakening both --sweeps A --levels gen10 \
    --out-dir experiments/configs-weakening-arbiter
```

Both arms of a pair run on the same host at the same seed, and the weakening filter is a final screen that draws no value from the `RandomSource`. The search is therefore bit-identical between the two arms of a pair, and they differ only in what survives step 6. Seed ranges across the two hosts are disjoint, so a host lost mid-campaign still leaves a balanced design over every family and both arms.

## 4. Endpoint

Fixed before launch. The response variable is `n_repairs`, with `found_repair` as its binary reading, paired by `(spec, seed)`.

No new metric is needed to identify the finding. Any repair the `wkoff` arm returns that its `wkon` twin does not is incomparable to the original by construction, because a weakening would have survived both arms of a bit-identical search. The paired difference is the measurement.

`best_relation` cannot serve here. It classifies a repair against an *ideal* drawn from `examples/*/fixes`, not against the input specification (`src/compare.cpp:366-368`), so it answers a question about repair quality rather than about comparability with what the engineer wrote.

## 5. Cost model

The `2026-08-10-arbiter-probe` archive gives the figures directly: on av2, over 40 seeds on these same 10 specifications at this operating point, `counter` cost 131 s per seed summed over the corpus. The worst single run was `amba` at 190 s against a 900 s cap, and nothing timed out.

Ten seeds per host across two arms is therefore about 44 min of `counter`, plus `compare` on top of it. This is a short campaign by the directory's standards.

Caps stay at arbiter-probe's flat 900 s rather than being cut to the measured costs. A cap that fires records `n_repairs = 0`, which is indistinguishable from the finding being looked for — a family where the unrestricted arm returns nothing.

## 6. Power, and what a null means

Against arbiter-probe's `wkon` yields, `prioritized-arbiter` ran 0 of 40 seeds and `full-arbiter` 1 of 40. The floor is low enough that a single repair in the `wkoff` arm is informative.

At 20 seeds, a true `wkoff` repair rate of 15% on a family shows at least one repair with probability 0.96. That is the sizing argument, and it is the whole of it.

This is a *reachability probe*. It answers whether incomparable repairs exist on a family, not at what rate they occur. A null at 20 seeds is not evidence of absence, and this plan commits in advance to reading it that way rather than reporting a family as closed.

## 7. Threats to validity

Recorded before the fact, so none of them is discovered afterwards in the shape of whichever result arrives.

**The fitness objectives under-price deletion.** The `wkoff` arm's output will skew towards gutted rewrites, and three separate mechanisms push it that way. `SimilarityMetric::Logarithmic` is the default (`include/config.hpp:109`) and compares growth rates, so it reads 1.0 regardless of how many traces a candidate deleted (`src/fitness/semantic_similarity.cpp:35-45`). A tombstoned requirement contributes a flat 0 to the semantic fold (`:228-231`), which makes two different removals indistinguishable to the objective. `status_score_mrs` normalises by the candidate's own live guarantee count (`src/fitness/status.cpp:263-265`) and returns `k_status_realizable` on an empty guarantee side (`:71-73`), so removing a guarantee can only raise the score. Read the `wkoff` arm for reachability, never for repair quality.

**The campaign measures the filter, not the search.** `run_weakening` is a final screen, so every repair the `wkoff` arm returns was reachable under the current search all along and was discarded at output. What a search freed from the restriction would find is a different quantity, and reaching it needs the fitness work above done first. Nothing here estimates it.

**The other correctness properties still hold in both arms.** The vacuity filter and the realizability gate run unchanged, so a `wkoff` repair is realizable, non-vacuous and well-separated. The arm admits incomparability alone, which keeps the finding narrow enough to attribute.

## 8. No decision rule

No accept/reject rule is registered, and this campaign moves no default. It feeds two decisions it does not make: whether `run_weakening` should be promoted from a filter to a reported tag on each repair, and whether the tier-1 fitness work in §7 is worth doing.

The finding is written as a count of `(spec, seed)` pairs where `wkoff` yielded and `wkon` did not, reported per family rather than pooled. Pooling would let one family's behaviour stand for the corpus, which is the failure `2026-08-10-arbiter-probe` was built to avoid.

## 9. Provenance

Branch `campaign/weakening-arbiter`. Profile `weakening-arbiter` in `scripts/run_experiments.py`. The declaration lives in `experiments/weakening-arbiter/campaign.toml`, which carries the branch, the per-host seed ranges and the phases, so the seed split exists in one place and is never chosen at the prompt.

Both hosts are staged from the branch head before launch, and `campaign.py stage` verifies that each binary reports the declared commit with `dirty=0`. Attribution is therefore `recorded` rather than inferred when the campaign closes.

Campaign directory `experiments/weakening-arbiter/`, tracked contents: this `PLAN.md`, `campaign.toml`, `PROVENANCE.json` written at launch, and `scripts/` — verbatim copies of the harness scripts that ran it, with blob shas recorded. The archived configs record only what the `weakening` factor overrides, so reproducing the campaign requires the commit `PROVENANCE.json` names.

A probe that measures what a filter discards is a smaller claim than one about what a different search would find. The distinction is the reason this plan registers no threshold, and the reason the fitness work has to come before any campaign that would.

## 10. Deviation from the registered corpus

The registered corpus of 10 `ARBITER_PROBE_SPECS` families is cut to 6, retaining `arbiter`, `arbiter-aurus`, `arbiter-handshake`, `full-arbiter`, `prioritized-arbiter` and `lily02`, and dropping `amba`, `round-robin-arbiter`, `simple-arbiter` and `load-balancer`. The cut was made on 2026-08-19, after 24 rows had been written, 9 on av2 and 15 on av3, and before any analysis of the endpoint.

The cause is the cost model in §5, drawn from the `2026-08-10-arbiter-probe` archive at 131 s of `counter` per seed summed over the 10 specifications, worst single run 190 s. That archive predates two changes to the shipped defaults, MRS status grading (measured at 1.89x cost) and `p_remove_guarantee`. Per-run cost on the campaign binary is roughly 6x what the model predicted, and `full-arbiter` took 65.5 s against a predicted 10.8 s. This is the "Config vintage" hazard the root `CLAUDE.md` records for archived configs, arriving in a cost model rather than in a config, where a changed C++ default silently changed what a prior campaign's timings predict.

`amba` hit the 900 s cap in both arms on every attempt, recording `timed_out = 1` and `n_repairs = 0`. §5 states that a cap which fires records `n_repairs = 0`, which is indistinguishable from the finding being looked for, so `amba` could not have contributed to the endpoint however long it ran. It accounted for about 88% of the campaign's wall time.

`round-robin-arbiter`, `simple-arbiter` and `load-balancer` were dropped on cost alone. All three already yield under `wkon`, at 100%, 82.5% and 92.5% of seeds in arbiter-probe, so they stood as controls rather than as the families the question is about.

The seed count is unchanged at 20 (av2 0–9, av3 10–19), as are the endpoint, the two arms and both zero-yield families, `full-arbiter` at 1 of 40 under `wkon` in arbiter-probe and `prioritized-arbiter` at 0 of 40. Power against a 15% `wkoff` rate stays at 0.96. What is lost is any statement about `amba`, and the breadth of the control set. The campaign can no longer say the restriction is free *across the arbiter corpus*, only across the six families it now covers.

The 24 rows already written stay in the CSV, `amba`'s timeouts included. They are a true record that `amba` is undecidable at 900 s on this engine, and `run_experiments.py` resumes on the natural key, so they are not re-run. The cut is by cost and by cap, made before any endpoint was read, and it removes no family on the basis of what its arms showed. A cost model is only as current as the defaults it was measured under.

## 11. Premise superseded

Section 2 frames the question around the arbiter families yielding close to nothing, citing the `2026-08-10-arbiter-probe` archive, where `full-arbiter` found a repair on 1 of 40 seeds and `prioritized-arbiter` on 0 of 40. On the campaign binary the same two families yield 20 of 20 and 18 of 20 under the `wkon` arm. The premise no longer holds.

The cause is the one section 10 records against the cost model. MRS status grading became the shipped default after 2026-08-10, measured at +8.9pp yield, and `p_remove_guarantee` was added in the same window. The second operator deletes guarantees, which is monotonically good for realizability. Both changes raise yield on exactly the families the plan selected for having none.

What the campaign measured is therefore a weaker claim than section 1 sets up. The registered question is whether the weakening restriction forecloses repairs on families that yield nothing. The measured result is that the restriction costs yield on families that already yield.

That result stands on its own terms. Of 120 pairs, 9 had `wkoff` yield where `wkon` did not, and none went the other way, giving a McNemar exact one-sided p of 0.00195. By family the 9 split as `lily02` 5, `arbiter` 3 and `prioritized-arbiter` 1, with `arbiter-aurus`, `arbiter-handshake` and `full-arbiter` tied at 20 of 20. `wkon` returned 250 repairs against `wkoff`'s 403. The endpoint in section 4 is unaffected, having been defined as the paired yield difference with no reference to the zero-yield baseline; only the motivation in sections 1 and 2 is stale. Read those two sections as the question as it stood on 2026-08-19 before the run, not as a description of the corpus the campaign ran against. A pre-registered plan dates from the engine it was written for, and recording where it has aged is cheaper than re-deriving the discrepancy later.
