# Sweep T: the monotone operators, and a decision rule overridden

Three arms over the 25 *Temporal Logic Synthesis Format* (TLSF) families of `H2H_TLSF_READY` at seeds 0 to 19, 1,500 rows. Every arm runs at `nsga2-apportion` with the log similarity metric, the weakening screen off, a 7200 s wall cap per specification, a `compare_timeout` of 1800 s and `jobs = 8`. `monooff` sets `p_monotone` and `p_clone_assumption` to 0 with `elitism_rate = 0.1`, `monoon` sets both probabilities to 0.25 with `elitism_rate = 0.1`, and `monoship` is `monoon` with `elitism_rate = 0`. All three set `accumulate_repairs = true`. The control arm is another campaign's archive by design: the 500 rows of `experiments/2026-08-21-aurus-h2h-ship`, paired on `(spec, seed)` over the same corpus, seeds, caps, hosts and seed split.

Every figure below reproduces with `python3 scripts/analyse_monotone.py experiments/2026-08-23-monotone --control ../2026-08-21-aurus-h2h-ship`, reading the merged CSV and the per-host CSVs that sit in this directory. `PLAN.md` beside this file pre-registered the arms, the four contrasts, the primary among them and the three outcomes of section 5, before any row existed.

Provenance is `6b78709` on branch `campaign/monotone`. All 1,500 rows record `commit=6b78709, dirty=0`, and both `counter` and `compare` on both hosts were built from it. av2 (`avlab12`) held seeds 0 to 9 for 750 rows and started 2026-08-22T03:41:21+0100; av3 held seeds 10 to 19 for 750 rows and started 03:46:32.

## The four contrasts

| contrast | family mean | families +/−/tied | p (family) | cluster mean | clusters +/−/tied | p (cluster) |
|---|---|---|---|---|---|---|
| control → `monooff` | 0.502 → 0.490 | 4 / 7 / 14 | 0.3818 (W+ = 22.5, n = 11) | 0.477 → 0.482 | 3 / 3 / 4 | 0.7500 (W+ = 12.5, n = 6) |
| **`monooff` → `monoon`** | 0.490 → 0.510 | 6 / 5 / 14 | 0.3525 (W+ = 44, n = 11) | 0.482 → 0.465 | 3 / 4 / 3 | 0.8906 (W+ = 15, n = 7) |
| `monoon` → `monoship` | 0.510 → 0.508 | 4 / 5 / 16 | 0.7188 (W+ = 26, n = 9) | 0.465 → 0.470 | 3 / 2 / 5 | 0.6250 (W+ = 10, n = 5) |
| control → `monoship` | 0.502 → 0.508 | 5 / 4 / 16 | 1.0000 (W+ = 22.5, n = 9) | 0.477 → 0.470 | 3 / 3 / 4 | 1.0000 (W+ = 11, n = 6) |

The run-level read is an exact two-sided *McNemar test* on `implies_ideal` over the paired cases. Control to `monooff` gained 22 and lost 28 at p = 0.4799; the primary gained 35 and lost 25 at p = 0.2451; `monoon` to `monoship` gained 28 and lost 29 at p = 1.0000; control to `monoship` gained 33 and lost 30 at p = 0.8013. No contrast separates on any read.

The `monoon` → `monoship` clustered read is uninformative by construction rather than by result. Its 5 non-tied clusters put the exact two-sided p floor at 2/2^5 = 0.0625, so no p under 0.05 exists there whatever the signs do.

## Per cluster on the primary

The two directions on the primary contrast disagree, and this is where that comes from.

| cluster | `monooff` | `monoon` | diff |
|---|---|---|---|
| arbiter | 0.758 | 0.833 | +0.075 |
| lily | 0.325 | 0.388 | +0.062 |
| humanoid | 0.013 | 0.075 | +0.062 |
| ltl2dba | 1.000 | 1.000 | 0.000 |
| detector-aurus | 1.000 | 1.000 | 0.000 |
| lift | 0.000 | 0.000 | 0.000 |
| rg | 0.525 | 0.500 | −0.025 |
| gyro | 0.100 | 0.050 | −0.050 |
| minepump | 0.800 | 0.750 | −0.050 |
| pcar-v2-888 | 0.300 | 0.050 | −0.250 |

Three clusters rise, four fall, and `pcar-v2-888` falls by −0.250 on its own. The Wilcoxon ranks by magnitude, so that one single-family cluster outranks every gain and carries the sign of the clustered statistic. This is an observation about where the signal sits and not a reason to discount the clustered read. Section 5 named that read as the tiebreaker in advance precisely so a look at which cluster moved could not rescue the result afterwards.

## The outcome, and the override

Section 5 registered three outcomes on the primary and added that where the per-family and clustered tests disagree, the clustered result governs. They disagree here. The per-family direction is positive at 0.490 → 0.510 and the clustered direction is negative at 0.482 → 0.465, so the clustered read governs and **outcome 3 fires: both defaults go to 0, with no re-run and no post-hoc arm**.

**That outcome was overridden.** `p_monotone` and `p_clone_assumption` both ship at 0.25. This is a deliberate departure from the pre-registered rule and is recorded here as one, in `PROVENANCE.json` and in the "Config vintage" note, rather than presented as something the data said.

Four things make the override defensible, and none of them make it a finding.

**Sweep T moves the two probabilities together in every arm.** No arm sits at (0.25, 0) or (0, 0.25). The primary therefore tests the pair as one factor, and outcome 3 would dispose severally of two operators the design only ever tested jointly.

**The two are unrelated moves.** `b9276a3` adds monotone weakening and strengthening rewrites of an existing formula; `3fd216a` makes `p_add_assumption` append a copy of a live ASSUME conjunct rather than a template one. They could point in opposite directions and cancel to the measured null, and this design cannot tell that apart from both being inert.

**Section 5 owes a probability sweep under outcomes 1 and 2 and owes nothing under outcome 3.** Two values that were argued rather than measured stop being measurable the moment the rule zeroes them. That asymmetry is a defect in the plan rather than a result from the data.

**The clustered reversal rides on one single-family cluster.** `pcar-v2-888` at −0.250 is the whole sign of a statistic taken over 7 non-tied clusters, 3 of which rise. That is where the signal sits, and the rule's own logic is why it cannot be the reason for the override.

Two measurements are owed after this, and both are consequences of the design rather than of the numbers. The first is the 2x2 that varies `p_monotone` and `p_clone_assumption` independently and separates the two operators. The second is the probability sweep section 5 already owed over the two 0.25 values.

## Against AuRUS, as context only

The head-to-head reading is not this campaign's endpoint and gates nothing. It runs the 2026-08-14 rule with AuRUS's rate filtered to well-separated repairs, over the same archived AuRUS rows.

| arm | mean family rate | p (family) | p (cluster) |
|---|---|---|---|
| AuRUS | 0.504 | — | — |
| control | 0.502 | 0.7549 | 0.5898 |
| `monooff` | 0.490 | 0.8069 | 0.6797 |
| `monoon` | 0.510 | 0.9072 | 0.4609 |
| `monoship` | 0.508 | 0.8898 | 0.4609 |

No arm separates from AuRUS at either read. The two clusters where AuRUS still leads outright are the two the branch was written for. `gyro` reads 0.383 for AuRUS against 0.050 on `monoon`, with `gyro-var2` alone at 0.700 against 0.100, and `lift` reads 0.300 against 0.000 on every counter arm. counter leads on `arbiter` at 0.850 for `monoship` against 0.633, and on `humanoid` at 0.075 for `monoon` against 0.000.

## Secondary measures, none gating

`monooff` yielded 467 of 500 runs with 33 timeouts at the 7200 s cap, a median wall time of 58.0 s, 108.5 machine-hours and a median of 32.0 repairs per run. `monoon` yielded 472 of 500 with 26 timeouts, 42.2 s, 97.1 hours and 29.0 repairs. `monoship` yielded 470 of 500 with 29 timeouts, 49.0 s, 101.1 hours and 30.0 repairs.

The `monoon` → `monoship` pair is the only measured comparison of `elitism_rate` at `nsga2-apportion`, and it reads 472 against 470 on yield with every quality read flat, for 4.0% more wall time at 0. The 2026-08-07 A/B found yield better at 0 on TLSF, 0.746 against 0.714 at p = 0.0002; that advantage does not replicate here.

## Threats and caveats

- **The two probabilities never vary independently.** Nothing here attributes anything to `p_monotone` or to `p_clone_assumption` alone, which is the first of the two owed measurements.
- **The control is another campaign's archive rather than an arm of this one.** The corpus, seeds, caps, hosts and split match, and both sides record their own commit, but the two counter arms differ in more than sweep T's factors because the control ran at a different binary.
- **`--pin-vintage` was deliberately not passed**, so `run_well_separation` and `allow_output_assumptions` take the binary defaults; `PLAN.md` records the values.
- **The clustered read has 7 non-tied clusters on the primary**, putting the exact two-sided floor at 2/2^7 = 0.0156. It needs near-unanimity across clusters to reach 0.05 at all.
- **`implies_ideal` is scored against counter's curated ideal set**, identically for every arm, so it bears on absolute rates rather than on the paired differences.

A pre-registered rule is worth having only where it is allowed to fire against the result somebody wanted, and this one fired and was then set aside. What makes that recoverable is that the rule, the reading it produced and the reason for going past it all sit in the same directory, so a later reader can weigh the override rather than discover it.
