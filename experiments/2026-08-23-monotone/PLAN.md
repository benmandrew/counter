# The monotone-operator branch: three arms, one contrast at a time

Pre-registered 2026-08-22, before any row of this campaign existed. Section 5 fixes the decision rule for the primary contrast and names it in advance, section 6 registers the hazards that could otherwise be discovered after the rates arrive, and both stay checkable against whatever the merged CSV holds.

## 1. Question

`feat/monotone-operators` is four commits off `adfb380`, three of which change the *Temporal Logic Synthesis Format* (TLSF) mutation grammar and one of which moves a default.

- **`f320d8c`, the `Implies` widening.** `pick_binary_kind` in `src/tlsf/mutation.cpp` drew from `{And, Or, Until, Release, WeakUntil}` and now also draws `Implies`. The FRETISH twins already drew it. There is no config key for this, so it is unconditional in the binary.
- **`b9276a3`, the monotone rewrite.** `tlsf_monotone_rewrite` is a new mutation arm that rewrites a section formula into one comparable to it under implication, weaker or stronger by a fair coin. Key `[tlsf.mutation] p_monotone`, binary default 0.25.
- **`3fd216a`, the cloned assumption.** `tlsf_add_assumption` may copy an existing live ASSUME conjunct rather than build from its template, and ordinary mutation then edits the copy. Key `[tlsf.mutation] p_clone_assumption`, binary default 0.25. The template emits at most seven nodes where the assumption-shaped ideals run to about thirty; `gyro-var2`'s sole ideal is 29 nodes.
- **`c0641ae`, the elitism default.** `elitism_rate` goes from 0.1 to 0.0.

The campaign asks what each of those is worth on repair quality, over the 25 TLSF families of `H2H_TLSF_READY` at seeds 0 to 19.

Both new probabilities are read before the `RandomSource` is touched, so at 0 they cost no draw and the breeding stream is byte-identical to a binary that never had them. The branch's `test_zero_probability_costs_no_draw` pins that. It is what lets an arm at 0 reproduce the binary without the operator rather than approximate it.

## 2. Why the existing rows do not answer it

`experiments/aurus-h2h-ship` runs counter at the 2026-08-21 shipping configuration plus the accumulator, from commit `3bb28dc`, over this same corpus and seed split. It holds none of the four commits. Its own control is the archived AuRUS rows of `experiments/2026-08-14-aurus-h2h`, and its endpoint is counter against AuRUS.

That campaign therefore measures a bundle against a second tool, and attributes nothing inside this branch. Three of the four commits are unmeasured code on any corpus.

The keys do not split the branch evenly. Two of the three grammar changes carry a probability and the `Implies` widening carries none, so an arm here can turn the rewrite and the clone off and cannot turn the widening off. The widening is attributable only against the archived control, across a binary change.

## 3. The arms and their configuration

Three arms, TLSF sweep T in `scripts/gen_configs.py`, stating four keys each.

| level | `p_monotone` | `p_clone_assumption` | `elitism_rate` | `accumulate_repairs` |
|---|---|---|---|---|
| `monooff` | 0.0 | 0.0 | 0.1 | true |
| `monoon` | 0.25 | 0.25 | 0.1 | true |
| `monoship` | 0.25 | 0.25 | 0.0 | true |

**Every key is stated rather than inherited**, for the reason sweep N states `accumulate_repairs` on both its arms: a silent key means one thing now and another after the next default move. `elitism_rate = 0.1` on the first two arms is what the control actually ran, the branch being what moves that default and `3bb28dc` not having it. `accumulate_repairs = true` on all three matches the control, which ran sweep N's `accon` level.

The generated `monooff` config differs from the control campaign's config in exactly two ways. It states `elitism_rate = 0.1`, which the control inherited at the same value. And it carries a `[tlsf.mutation]` section pinning `p_assumption = 0.3` and `p_temporal = 0.2`, both binary defaults, alongside the two new keys at 0.

The corpus is `H2H_TLSF_READY`, 25 families, at seeds 0 to 19 split av2 0-9 and av3 10-19. The wall cap is 7200 s at `jobs = 8`, with a `compare_timeout` of 1800 s. Three arms over 25 families and 20 seeds is 1500 runs, 750 per host.

`--pin-vintage` is deliberately not passed, for the reason `aurus-h2h-ship`'s plan records: it writes `run_well_separation` from `gen_configs.DEFAULTS`, whose entry has read true since the binary default went false in `b101ada` on 2026-08-10, and pinning would put the per-generation well-separation filter into arms whose control ran without it.

## 4. The control and the four contrasts

The outer control is the archived `aurus-h2h-ship` rows: same corpus, same seeds, same 7200 s cap, same `compare_timeout`, same two hosts, same seed split, same `jobs = 8`. No fourth arm runs here.

Every contrast is paired on `(spec, seed)`.

- archived control → `monooff` — the `Implies` widening alone, across a binary change.
- `monooff` → `monoon` — the monotone rewrite plus the cloned assumption, within this campaign's binary.
- `monoon` → `monoship` — the elitism default move.
- archived control → `monoship` — the branch as it ships.

## 5. Endpoint and decision rule

The primary endpoint is per-run `implies_ideal`, scored by `compare` against `examples/<spec>/fixes`, aggregated to a per-family rate over the 20 seeds and read with a two-sided exact *Wilcoxon signed-rank test* over families at alpha 0.05. A second read runs the same test over the correlated-family clusters. `scripts/analyse_aurus_h2h.py` is the existing analysis and prints an exact two-sided p floor of 2/2^n at every n, so below 6 non-tied units no p under 0.05 exists at all.

**The primary contrast is `monooff` → `monoon`.** It is the one this branch was written to test, and it is the only one whose two sides come from the same binary. The other three are secondary and are reported without an alpha correction pretending they were independent: three contrasts over one corpus share families, seeds and ideals, so their family-level reads correlate by construction, and a correction calibrated for independent tests would misstate the multiplicity in both directions.

Three outcomes on the primary contrast, and the campaign takes exactly one.

1. **`monoon` higher** at p < 0.05: both operators keep their 0.25 defaults.
2. **Direction positive, p ≥ 0.05**: both operators keep their defaults and a tuning campaign over the two probabilities is owed and recorded as owed.
3. **`monoon` lower**: both defaults go to 0, with no re-run and no post-hoc arm added to recover the direction.

The two 0.25 values are argued rather than measured. Neither probability has ever been tuned, and no arm here varies either one, so outcome 1 leaves the same probability sweep owed that outcome 2 does. A win says the operators earn their place at one operating point.

Where the per-family and clustered tests disagree, the clustered result governs, following the head-to-head's section 10.2.

## 6. Registered hazards

**A better arm can present a weaker p.** Improving counter ties more families at the ceiling, and the Wilcoxon drops a tied family from the test, so n shrinks as the arm gets better. `aurus-h2h-ship` registered the same hazard. Registered here in the same form: the sign counts, the tie count and both mean rates are reported alongside every p value, and outcome 2 is read against them rather than on its own.

**The `Implies` widening rides in every arm.** No arm here can turn it off, so it is confounded with the binary change in the archived-control contrasts and invisible in the internal ones. A null primary contrast condemns the rewrite and the clone, and says nothing about the widening.

**Two of the three grammar changes grow formulae.** The 2026-08-14 audit measured `humanoid-531` losing runs to the earlier operator repairs, being the family that builds much larger formulae. A rewrite arm and a clone arm both grow formulae. The bloat-cap drop rate is reported per family for `humanoid-531` and for `arbiter-aurus`, where a per-section bloat baseline was measured raising the drop rate from 1.6% to 9.4% and costing that family 9 of its 10 ideal-implying runs. The cap keeps its specification-wide baseline here. A filter tightening that fights an operator loosening is the general shape being watched for.

**Three of the four commits are unmeasured code.** The elitism move has its own contrast and the widening has one against the archived rows; neither is what the primary endpoint tests. Reading the primary result as a verdict on the branch would attribute to two operators what four commits produced.

## 7. Secondary measures, none gating

Yield, as the count of runs emitting at least one repair. `n_repairs` per scoring run, which the accumulator raises on all three arms. The median paired wall ratio against the archived control, and against `monooff` for the internal contrasts. The timeout rate at the 7200 s cap. The `compare` timeout count, which the 1800 s budget exists to keep at zero. The bloat-cap drop rate of section 6, per family. The paired counter-against-counter comparison on `implies_ideal` by exact *McNemar test*, which is the run-level read of each contrast where the family-level Wilcoxon is the registered one.

## 8. Threats to validity

**The control is another campaign's archive.** It rests on the seed split, the caps and the corpus staying where they were put, which is why `campaign.toml` repeats the split rather than choosing one, and why section 3 writes down what `monooff` states that the control inherited. Moving the split would pair av2 rows against av3 ones without any error.

**The archived rows came off a different binary.** The `monooff` contrast against them is a paired read across a build, so anything else that changed between `3bb28dc` and `adfb380` lands in that contrast. The internal contrasts do not carry it.

**Host load is uncontrolled.** The two campaigns run on the same two machines and not at the same time. The wall-ratio secondary of section 7 cannot separate load from the arms; the primary endpoint compares rates rather than runtimes and does not need to.

**The ideals are a curated set.** Every arm is scored by the same `compare` against `examples/<spec>/fixes` with no `-ref` flags, so an incomparable family biases `implies_ideal` equally across arms and the paired contrasts are unaffected by which families those are.

**`run_weakening = false` means a written repair may forbid behaviour the original allowed.** That is the shipping default on all three arms and on the control, so it is constant across every contrast.

## 9. Cost

One phase, three arms inside it, 750 runs per host. `aurus-h2h-ship` took roughly 4.5 hours per host for one arm over this corpus at `jobs = 8`, which puts this campaign at about 13 hours per host. The RAM argument behind `jobs = 1` on the other TLSF profiles was never measured on this corpus: a 24-run calibration peaked at 18.7 GB of 125 GB.

## 10. Provenance

Branch `campaign/monotone`, declared in `campaign.toml` beside this file. `scripts/campaign.py stage` puts each host on the branch and rebuilds it, `start` launches the phase, and both read the declaration, so the seed split exists in one place and is never chosen at the prompt. The configs are generated on the host at stage time by the declaration's `configs` line, the branch carrying the declaration rather than the untracked config tree. The profile is `monotone` in `scripts/run_experiments.py`, registered in `merge_experiments.PROFILE_CSVS` as `results-monotone.csv`.

On close, vendor `gen_configs.py`, `run_experiments.py`, `merge_experiments.py` and `analyse_aurus_h2h.py` into `scripts/` beside this plan with their blob shas in `PROVENANCE.json`, and record whether the branch merged or was split.

Three defaults move with this branch, and all three are already written into the "Config vintage" note in `experiments/README.md` — the branch carries those entries rather than owing them at merge. `p_monotone` and `p_clone_assumption` default on at 0.25 from `b9276a3` and `3fd216a` onward, and `elitism_rate` moves at `c0641ae`. Every campaign archived before 2026-08-21 omits all three keys and now means something it did not, so reproducing one requires writing `p_monotone = 0.0`, `p_clone_assumption = 0.0` and `elitism_rate = 0.1` back by hand.

Naming the primary contrast before the rates exist is what keeps three reads over one corpus from becoming a search for the one that clears 0.05. The harder registration is section 5's last clause: two probabilities that have never been tuned leave a sweep owed whichever way this campaign lands, and a win is the easier result to stop measuring after.
