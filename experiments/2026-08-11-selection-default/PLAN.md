# Selection default: should `nsga2-apportion` replace `nsga2-truncate`?

Pre-registered 2026-08-11, before any row is collected. Written to bind the decision rule to the result it is meant to decide.

## 1. Question

`Config::selection_scheme` ships as `Nsga2Truncate`. Should it ship as `Nsga2Apportion`?

This is the question `2026-07-31-replicate` asked and failed to settle, and the one `2026-08-10-arbiter-probe` was explicitly built *not* to reopen. It is asked here on its own terms, with the arm that both earlier campaigns lacked or could not use.

## 2. What is already known, and what it does not settle

`2026-07-31-replicate` pre-registered four criteria for making the scheme the default and failed two: the pooled `implies_ideal` interval and the median paired wall ratio, which came in near 3.0 against a bound of 2.0. It also failed to beat its compute-matched arm C.

`2026-08-10-arbiter-probe` changed one of those facts and confirmed another:

- **Cost is no longer the obstacle.** At gen10/pop200 the median paired wall ratio is 1.13 and the aggregate 1.21, against ~3.0 at gen40/pop1000. The replicate cost was traced to the serial vacuity filter and the final realizability collect, both of which scale with generations and population, so the ratio is a function of the operating point rather than of the scheme. At the shipped operating point apportion is close to free.
- **Breadth is the obstacle.** The `arbiter` unlock reproduces at full strength (0.000 against 0.950, Holm-corrected p < 0.0001) but appears in 1 of 9 arbitration families.

Neither campaign licenses a default change, and the arbiter probe cannot be repurposed into one for three reasons that this campaign exists to remove:

1. **Its corpus was selected for the effect.** Nine of ten specs are arbitration, chosen because `arbiter` is where the signal was. An effect measured on a corpus enriched for it is not an estimate of that effect on the corpus a user has.
2. **It has no FRETISH data at all.** `selection_scheme` governs both paths.
3. **It has no compute-matched arm**, so it cannot distinguish "apportion searches better" from "apportion searches longer".

Scored after the fact against replicate's four criteria, the arbiter probe passes two (yield, cost), fails the pooled `implies_ideal` margin on the nine-family pool (−0.0111, 95% CI [−0.0500, +0.0278]), and leaves the compute-matched criterion untestable.

## 3. Design

Three arms, fully paired by `(spec, seed)`.

| arm | scheme | generations | role |
|---|---|---|---|
| A | `nsga2-truncate` | shipped | the incumbent default |
| B | `nsga2-apportion` | shipped | the candidate |
| C | `nsga2-truncate` | scaled to B's measured wall cost | compute control |

Arm C is what makes the result a claim about the *scheme* rather than about the compute it consumes. Its generation count is not guessed: it is set from the wall ratio measured in calibration (§5) on this campaign's own corpus, rounded up to the next integer generation. If B costs 1.2× A, arm C runs `ceil(10 × 1.2) = 12` generations.

Everything else is held at the shipped configuration. `elitism_rate` is pinned to 0.1 in the emitted config rather than inherited, since `2026-08-07-elitism` closed on cost alone and a later change to that default would otherwise silently restate what this campaign ran under.

## 4. Corpus

Deliberately **not** enriched, which is the methodological correction this campaign makes to the arbiter probe.

- **FRETISH:** all four specs (`takeoff`, `fsm`, `fsm-timing`, `fsm-combined`), 70 seeds → 280 pairs per contrast.
- **TLSF:** the full 20-family ablation corpus, 25 seeds → 500 pairs per contrast.

Both sample sizes come from §7's power calculation, not from convenience. `arbiter` sits inside the TLSF corpus as one family of twenty and receives no special weight; if the pooled effect is carried by it alone, the per-spec breakdown in §6 will say so.

## 5. Cost and calibration

Measured per-run means: 24.3 s on FRETISH and 37.6 s on the 20-family TLSF corpus, both from `2026-08-07-elitism` at this operating point and `jobs = 1`. Arms B and C each cost about 1.2× arm A.

Projected: 840 FRETISH runs and 1,500 TLSF runs, ≈ 24 h total, ≈ 12 h per host across av2 and av3. An overnight campaign.

**Calibrate before launching anyway.** The wall ratio sets arm C's generation count, so it is a design parameter here rather than a convenience estimate, and the arbiter probe's 1.13 was measured on an arbitration-heavy corpus that is not this one. Time a 60-run slice — arms A and B, six families spanning the cost range, five seeds — on one host and take the ratio from that. Time it with `jobs` well above the worker count: a prior campaign timed 4 jobs on 4 workers and underestimated by about 27%.

If calibration puts the ratio above 2.0, stop and do not launch: criterion 5 fails before any row is collected, and the campaign has nothing to add.

## 6. Analysis

Paired by `(spec, seed)` throughout. Two contrasts, B vs A and B vs C, computed independently and reported side by side; the two paths are analysed separately and never pooled, since `implies_ideal` means different things against different ideal sets.

- **Quality:** mean paired difference in `implies_ideal` with a bootstrap 95% CI (10,000 resamples), pooled within path, plus the same per spec.
- **Yield:** `found_repair` by McNemar's exact test per spec, and Cochran–Mantel–Haenszel stratified by spec for the pooled effect.
- **Repair count:** mean paired difference in `n_repairs`.
- **Cost:** median paired wall ratio, Wilcoxon signed-rank.

Rows with `compare_timed_out = 1` are excluded and their count reported; the exclusion is symmetric, dropping the whole pair rather than the row, because an unpaired drop is what biased the discarded `df66e44` execution of the arbiter probe.

Per-spec McNemar with Holm correction is **reported but does not gate**. It exists to answer "which specs, if not all of them" without letting that question steer the primary.

The analysis is `scripts/analyse_selection_default.py`, committed here **before launch** alongside this plan rather than written once the rows exist. A rule fixed in prose but implemented after the fact is only half pre-registered: every judgement call the prose leaves open — how a pair with one timed-out comparison is handled, whether criterion 4 reads the point estimate or the interval — gets made with the data visible. The script evaluates all five criteria itself and prints the decision, so the analyst's remaining discretion is which CSV to pass it. Its bootstrap seed is fixed for the same reason: an interval that can be resampled is an interval that can be resampled until it agrees.

It is validated against a synthetic dataset with a planted effect — B beating A on yield while tying C — and recovers exactly that, reporting "the gain is compute, not scheme". That is the discrimination `2026-07-31-replicate` could not make and `2026-08-10-arbiter-probe` had no arm for, so it is the one worth testing before trusting.

## 7. Decision rule

Fixed before launch.

### The non-inferiority margin is 0.05

On the pooled paired `implies_ideal` mean difference, per path: the 95% CI lower bound must exceed **−0.05**.

This matches `2026-08-07-elitism`, the most recent default decision, and it is chosen over replicate's −0.02 on grounds of attainability rather than taste. From the measured paired-difference standard deviations — 0.341 on FRETISH and 0.418 on TLSF — the pairs needed for the interval to clear the margin are:

| margin | FRETISH, true effect 0 | at −0.01 | TLSF, true effect 0 | at −0.01 |
|---|---|---|---|---|
| 0.02 | 1,117 | 4,468 | 1,679 | 6,713 |
| 0.03 | 497 | 1,117 | 746 | 1,679 |
| **0.05** | **179** | **280** | **269** | **420** |

A margin of 0.02 is not a stricter standard, it is an unreachable one. It demands 1,117 FRETISH and 1,679 TLSF pairs even if the true difference is exactly zero, and today's TLSF point estimate is −0.011, at which it demands 6,713 TLSF pairs — about 13,400 runs for that path alone. A rule that cannot conclude for any small adverse effect does not protect the default; it just guarantees the campaign ends in "inconclusive" and the question gets asked again.

**The protection against a loose pooled margin is the rest of the rule, not a smaller number.** A pooled mean hides per-spec damage: in the arbiter probe, `arbiter-aurus` fell 0.35 at unchanged yield while the pooled figure including the control read +0.030. Hence criterion 2.

### `nsga2-apportion` becomes the `config.hpp` default only if all five hold

1. **Quality non-inferior, against both A and C.** Pooled paired `implies_ideal` 95% CI lower bound > −0.05, on FRETISH and on TLSF, for both contrasts. Four intervals, all four must clear.
2. **No spec badly damaged.** No spec whose mean paired `implies_ideal` is below −0.15 with a 95% CI excluding zero, against either A or C. At 25–70 seeds the per-spec CI half-width is ≈ 0.10–0.13, so this is detectable rather than decorative.
3. **Yield improves, against both A and C.** CMH-stratified odds ratio on `found_repair` above 1 with a CI excluding 1, on each path, for both contrasts. Non-inferiority is not enough for a default change: the reason to move must be a gain.
4. **`n_repairs` does not fall** on either path, against either arm.
5. **Cost bounded.** Median paired wall ratio B/A ≤ 2.0, retained from replicate. Arm C is the real cost control; this criterion is what a user's wall clock cares about.

Beating arm C is criterion 3's second half and is the criterion both prior campaigns could not satisfy or could not test. If B beats A but not C, the finding is that generations are the lever and the scheme is not — a documentation result about the operating point, not a default change.

### Deliverables

- *All five hold*: the default changes in `include/config.hpp`, `example-config.toml` and `docs/configuration.rst`, each citing this campaign, with the measured quality and cost deltas stated in the same place.
- *Criterion 3 fails against C alone*: `docs/configuration.rst` records that the gain is compute, not scheme, and the generations default is opened as a separate question.
- *Any other failure*: the scheme stays opt-in and `docs/configuration.rst` gains the narrow guidance the arbiter probe earned — apportion for `examples/arbiter`, where it is the difference between 0/40 and 38/40 at a 1.11× wall cost — with the breadth result stated so the recommendation is not read as general.

### Not covered

Whether the mechanism is diversity; whether `1 / (1 + rank)` is the right weighting; the interaction with `elitism_rate`, pinned here rather than crossed; and the `WeightedAverage` scheme, which is kept for comparison rather than use.

## 8. Launch

Both hosts idle before launch — `wall_time_s` is a response and arm C is defined by it, so nothing may co-schedule.

Configs must pin every key that has crossed a default line, or the archive will not reproduce: `allow_output_assumptions` and `run_well_separation` have each moved twice, and `b101ada` additionally changed what the status scale means in a way no config key restores. Record the binary commit; the campaign is not reproducible from the configs alone.

Execution order seed-major, then spec, then arm, so a kill at any deadline yields a balanced design at fewer seeds rather than a complete arm A and an empty arm C. Disjoint seed ranges per host. Launch detached, guard on `ps comm` rather than `pgrep -f`, and run the `ltl2tgba` orphan janitor for the duration.

Fresh CSV and results directory: resume skips by CSV key and never cleans output directories, so a stale row survives an engine change.

## 9. Provenance

Record at launch, not after: binary commit and `dirty` from `counter --version` on each host, per-host launch manifests, vendored `scripts/`, and an annotated `provenance/` tag if the branch is not merged. The arbiter probe needed three executions, two of them discarded for engine changes landing mid-flight; freeze the engine for the duration or expect the same.
