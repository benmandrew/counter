# Sweep U: whether assumption construction reaches three families counter has never repaired

Pre-registered 2026-08-25, before any row of this campaign existed. Section 5 fixes the primary contrast, its power arithmetic and its three outcomes; section 6 registers what the design cannot attribute, and section 7 the hazards that would otherwise be discovered once the rates arrive. All of it stays checkable against whatever the merged CSV holds.

## 1. Question

`feat/assumption-construction` adds five keys to the *Temporal Logic Synthesis Format* (TLSF) mutation and crossover grammar. Every one ships at a no-op value, and every one was argued rather than measured.

- **`max_assumption_width`** (no-op 1, armed 3). The body of an appended assumption is drawn from the grammar `term := [F](literal & ... & literal)`, `body := term | ... | term` rather than being a single literal. At width 1 the grammar emits exactly the single literal it emitted before. The wider grammar is what puts `lift`'s ideal `G F (b1 | b2 | b3)` and `humanoid-503`'s `G F (!m0 & !m1 & m2 & !button)` inside reach, along with the nested shape `gyro-var2` needs.
- **`p_bare_assumption`** (no-op 0.0, armed 0.25). An appended unconditional assumption may be `F body` rather than `G F body`. `lily11`'s entire ideal is a bare `F req`, and `G F req` is strictly stronger, so no rewriting of a G-wrapped assumption reaches it.
- **`p_remove_assumption`** (no-op 0.0, armed 0.05). One live ASSUME conjunct is deleted. It mirrors `p_add_assumption`, which had no counterpart, and five of the corpus's ideals replace an assumption rather than adding beside it.
- **`p_union_assumption`** (no-op 0.0, armed 0.25). Crossover may union a whole live ASSUME conjunct from the second parent instead of grafting a subformula. This is AuRUS's level-1 move, which counter's one-conjunct-per-side graft cannot express. It is assumption-side only, the guarantee side pairing by position with the original.
- **`p_burst_continue`** (no-op 0.0, armed 0.5). Mutation applies `1 + Geometric(p)` edits rather than one, capped at 8.

The burst draws from a *geometric distribution* rather than a power law because the width it models is measured. Over the 40 ideals under `examples/` whose delta parses, edit width runs 0.475 at one slot, 0.200 at two, 0.200 at three and 0.125 at four or more. `1 + Geometric(0.5)` fits that at a *Kullback-Leibler divergence* (KL) of 0.066, against 0.163 for a power law at beta = 1.5.

Each key is a no-op at the stated value **and costs no `RandomSource` draw there**, so the `reachoff` arm reproduces a binary that never had any of the five rather than approximating one. `test/tlsf/assumption_tests.cpp` asserts that each key draws only when armed, and the absolute draw-count golden in `test/tlsf/monotone_tests.cpp` pins the all-off stream.

## 2. Why the existing rows do not answer it

`experiments/2026-08-23-monotone` ran three arms over the 25 families of `H2H_TLSF_READY` at seeds 0 to 19. Three families came out at or near zero across all three arms: `gyro-var1` at 0 of 60, `gyro-var2` at 8 of 60 and `lift` at 0 of 60 on `implies_ideal`. Every run in those families emitted repairs, 20 of 20, at a median of 20 to 34 each. So the search produced output on every run and none of it implied an ideal. The limit is the formula shape.

The same corpus puts AuRUS ahead on both clusters. `gyro` reads 0.383 for AuRUS against 0.050, and `lift` reads 0.300 against 0.000. Those are the two clusters the monotone branch was written for and did not move.

No archived row varies any of the five keys, because no archived binary has them. The pre-2026-08-25 grammar is recovered here as an arm rather than borrowed from an archive.

## 3. The arms and their configuration

Sweep U in `scripts/gen_configs.py`, a 3x2 cross of an operator factor against a search factor, six arms, every case paired on `(spec, seed)`.

| level | `max_assumption_width` | `p_bare_assumption` | `p_remove_assumption` | `p_union_assumption` | `p_burst_continue` | generations | population |
|---|---|---|---|---|---|---|---|
| `reachoff-s` | 1 | 0.0 | 0.0 | 0.0 | 0.0 | 10 | 200 |
| `reach-s` | 3 | 0.25 | 0.05 | 0.25 | 0.0 | 10 | 200 |
| `reachburst-s` | 3 | 0.25 | 0.05 | 0.25 | 0.5 | 10 | 200 |
| `reachoff-l` | 1 | 0.0 | 0.0 | 0.0 | 0.0 | 40 | 400 |
| `reach-l` | 3 | 0.25 | 0.05 | 0.25 | 0.0 | 40 | 400 |
| `reachburst-l` | 3 | 0.25 | 0.05 | 0.25 | 0.5 | 40 | 400 |

**Every key is stated rather than inherited**, following sweep T's argument: a silent key means one thing now and another after the next default move. All six arms set `accumulate_repairs = true`, run at `nsga2-apportion` with the log similarity metric and the weakening screen off, and take a 7200 s wall cap per specification with a `compare_timeout` of 1800 s at `jobs = 8`. Those are the shipping values and the values the monotone campaign ran, so they are constant across every contrast here.

`--pin-vintage` is deliberately not passed, for the reason both prior declarations record. It writes `run_well_separation` from a `gen_configs.DEFAULTS` entry that does not track the binary default, which would put the per-generation well-separation filter into arms that are meant to run without it.

## 4. The corpus: 23 families, not 25

`humanoid-531` and `humanoid-742` are excluded from all six arms. Both are limited by time rather than by grammar. `humanoid-742` timed out in 60 of 60 monotone runs and `humanoid-531` in 28 of 60, both at the 7200 s cap, and `humanoid-742`'s ideal `G F (!obstacle)` is already expressible under the grammar as it stands at width 1. Between them they would take 44% of this campaign's wall clock to measure a rate no arm here can move.

This is a budget decision taken in advance and recorded as one. Anything that reads across the two campaigns reads over 23 families on this side, and the excluded pair is named rather than dropped quietly at analysis time.

## 5. Endpoint and decision rule

**Amended 2026-08-25, before any result was read.** The seed count rises from eight to twelve. The campaign was already running when the decision was taken, at 27 of 552 rows on av2 and 29 of 552 on av3, and no `implies_ideal` value from this campaign had been read by anyone at that moment. That ordering is the whole justification. Extending a design after seeing its result is *optional stopping*, which inflates the false-positive rate a pre-registration exists to hold down. Extending it blind is a design amendment and costs nothing. The amendment is recorded here, before any result exists, so a later reader can check the claim rather than take it on trust.

The endpoint, the primary contrast, the three outcomes and the regression gate are unchanged, and only the counts move. Twelve seeds over the three target families give 36 paired cases rather than 24. The six-discordant-pair floor stays where it was, being a property of the exact test (2/2^6 = 0.031) rather than of the sample size. A smaller true effect now suffices to produce six of them. The operators must repair a sixth of target runs to register, 6 of 36, where eight seeds required a quarter. The regression gate over the other 20 families grows from 160 paired runs to 240.

The top-up is a second phase in `campaign.toml` carrying its own `hosts` table (av2 takes seeds 8 to 9, av3 takes 10 to 11), rather than a change to the campaign-level split, which would repoint seeds already written against the old one. Seeds 8 to 11 are new, so the phase resumes rather than re-runs, `(spec, seed)` being in the resume key. A queued second entry waits on its own, since a tick refuses to stage a host with a live `counter` process.

*The two paragraphs below are the original registration, unedited. Their seed count and paired-case count are superseded by the amendment above; everything else in them stands.*

**The primary contrast is `reachoff-s` to `reach-s`, confined to `gyro-var1`, `gyro-var2` and `lift`.** That is 24 paired cases at eight seeds. The endpoint is per-run `implies_ideal`, scored by `compare` against `examples/<spec>/fixes`, read with an exact two-sided *McNemar test* over the discordant pairs at alpha 0.05.

The power arithmetic is stated here rather than derived afterwards. Exact McNemar needs six discordant pairs all pointing one way before any p under 0.05 exists at all: 2/2^6 = 0.031, where five discordant pairs put the floor at 2/2^5 = 0.0625. The control arm sits at zero on two of the three target families and at 8 of 60 on the third, so a discordant pair is close to being a `reach` success counted on its own. Eight seeds over three families therefore require the operators to repair a quarter of target runs before the test can register anything. Six seeds would require a third and four seeds a half. That is why the pilot is eight seeds and not fewer.

The primary is not pooled over all 23 families, and the reason is a reading error already paid for. An effect confined to three families is null by construction in a test taken over twenty-three, and `experiments/2026-08-23-monotone/REPORT.md` records that costing the monotone campaign its conclusion. The pooled read appears below as a regression gate, which is the job it can do.

Three outcomes on the primary, exactly one taken.

1. **`reach-s` higher at p < 0.05.** The four assumption-construction keys ship armed, and the 2^4 that separates them is owed and recorded as owed.
2. **`reach-s` repairs at least one target run and `reachoff-s` none, at p >= 0.05.** The shape is reachable and the pilot cannot size the effect. The four keys ship armed provisionally, and a full campaign at twenty seeds is owed before either the direction or the magnitude is relied on.
3. **`reach-s` repairs no target run.** The four keys stay at their no-op values, with no re-run and no post-hoc arm.

Three secondary contrasts, reported without an alpha correction pretending they were independent. Three contrasts over one corpus share families, seeds and ideals, so their reads correlate by construction and a correction calibrated for independent tests would misstate the multiplicity in both directions.

- `reach-s` to `reachburst-s` — the burst alone, judged on the same three outcomes on its own account.
- `reachoff-s` to `reachoff-l` — search size, taken over all 23 families, since a capacity change is not a targeted one.
- `reach-l` to `reachburst-l` — whether the burst's effect depends on search size.

**The regression gate.** If the 160 paired runs of the other 20 families show `reach-s` losing `implies_ideal` against `reachoff-s` at exact McNemar p < 0.05, the keys stay at their no-op values regardless of what the primary says. A loss there is a live possibility rather than a formality. A wider assumption body is a larger conjunct, and the bloat cap measures each conjunct against the largest formula anywhere in the original, so an operator loosening and a filter tightening meet directly. The 2026-08-14 audit measured `humanoid-531` losing runs to the earlier operator repairs on exactly that mechanism.

## 6. The bundling, registered

The `reach` arm turns four keys on together. Nothing in this design attributes anything to one of them alone, and the 2^4 that would is owed under outcomes 1 and 2. This is sweep T's defect repeated knowingly, and `experiments/2026-08-23-monotone/REPORT.md` is where the cost of it is written down.

It is tolerable here where it was not there, because the corpus separates the keys without a factorial. `lily11`'s ideal is reachable only through `p_bare_assumption`, `lift`'s only through `max_assumption_width`, and `lily02`'s `lilydemo05` only through the burst. A per-family read therefore carries information the sweep itself does not.

That reading is descriptive and cannot be used to rescue outcome 3. If no target run is repaired, the per-family attribution has nothing to attribute, and reaching for it afterwards would turn a registered rule into a search for the family that moved.

## 7. Registered hazards

- **All five defaults are argued rather than measured.** The campaign tests whether the operators reach anything, not whether the armed values are the right ones.
- **Eight seeds cannot estimate a rate.** A target family moving from 0 of 8 to 1 of 8 is a reachability signal and not an effect size.
- **Pairing is on the case, not the trajectory.** The two arms of a contrast share `(spec, seed)`, but their `RandomSource` streams diverge from the first differing draw, so the pairing controls the subject and the seed and nothing downstream of them.
- **The two removal branches cost the rewrite path a little.** `p_add_assumption` and `p_remove_assumption` are both early returns, so each slightly reduces how often a mutation reaches the rewrite. Together at 0.05 they leave it at 0.9025 against 0.9500, a 5% relative loss. Registered as measured and expected not to matter at this resolution.
- **The large arms change two things at once.** `l` moves generations from 10 to 40 and population from 200 to 400, so the search factor is a capacity contrast and not a decomposition of the two.
- **`implies_ideal` is scored against counter's curated ideal set**, identically across arms, so it bears on absolute rates rather than on the paired differences.

## 8. Secondary measures, none gating

Yield, as the count of runs emitting at least one repair. The timeout rate at the 7200 s cap. Median wall time per arm and median `n_repairs` per arm, both of which the accumulator raises on all six. The node-size distribution of appended assumptions, wherever the accumulated output allows it to be recovered, which is the direct read on whether `max_assumption_width` produces the larger bodies it was written for.

## 9. Cost

One phase, six arms inside it, 23 families at eight seeds is 1,104 runs, 552 per host. The estimate is 14.9 hours per host, taken from the 2026-08-23 campaign's per-family mean wall times with the three large arms costing eight times the three small ones and the two saturating families removed. The campaign fits a 36-hour window with margin rather than filling it.

**Amended 2026-08-25.** The twelve-seed design is 1,656 runs, 828 per host. The top-up phase carries the four added seeds alone, 552 runs at 276 per host, and adds roughly 7.5 hours to each host on the same per-family means. The two figures are 14.9 hours per host for the original eight seeds and 22.4 hours per host for all twelve, against a budget of 36 hours of exclusive access to both machines. The margin narrows and holds.

## 10. Provenance

Branch `campaign/assumption-reach`, declared in `campaign.toml` beside this file, which is what makes the seed split reproducible. `scripts/campaign.py stage` puts each host on the branch and rebuilds it, `start` launches the phase, and both read the declaration, so av2 takes seeds 0 to 3 and av3 takes 4 to 7 without a range ever being chosen at a prompt. The split gives av2 the low half, as every campaign on this corpus has, so a later top-up to twenty seeds resumes rather than re-runs. The profile is `assumption-reach` in `scripts/run_experiments.py`, registered in `merge_experiments.PROFILE_CSVS`.

Both sides of every contrast are arms of this campaign at one binary. The monotone campaign paired against another campaign's archive, built from a different commit, and carried everything that changed between the two builds into its control contrast. Nothing here carries that.

On close, vendor `gen_configs.py`, `run_experiments.py`, `merge_experiments.py` and the analysis script into `scripts/` beside this plan with their blob shas in `PROVENANCE.json`, and record whether the branch merged or was split. Five defaults move with this branch and all five belong in the "Config vintage" note in `experiments/README.md`, since every campaign archived before 2026-08-25 omits all five keys and now means something it did not.

Naming the target families and the outcome rule before any row exists is what stops a pilot this small from becoming a search for the family that happened to move. The harder registration is section 6's last clause, because a bundle of four keys with a per-family story available for each of them is a design that can explain any result after the fact.
