# `weighted` against `nsga2-apportion`: level on quality, 2.673x the cost

Two arms over 21 *Temporal Logic Synthesis Format* (TLSF) families at seeds 0 to 15, 672 rows and 336 complete `(spec, seed)` pairs. Both arms run at gen 10 / pop 200, sweep T at level `monoon`, the weakening screen off (`wkoff`), the log similarity metric and `jobs = 8`. The aggregate fitness weights are 0.1 / 0.2 / 0.7 over syntactic, semantic and status, for the reason `PLAN.md` section 4 gives. `Config::selection_scheme` is the only config key that differs between the two arms, `nsga2-apportion` as control and `weighted` as treatment.

Every figure below reproduces with `python3 scripts/analyse_selection_smoke.py experiments/2026-08-26-selection-smoke --control ../2026-08-23-monotone`, reading the merged CSV, the per-host CSVs and the per-run manifests that sit in this directory. `PLAN.md` beside this file pre-registered the arms, the endpoints, the fitness weights, the corpus exclusions and the threats of section 8 before any row existed. It registered no decision rule, and section 7 says why.

Provenance is `921ce1b` on branch `campaign/selection-smoke`. All 672 rows record `commit=921ce1b, dirty=0`. av2 held seeds 0 to 7 and started 2026-08-26T21:21:20+0100; av3 held seeds 8 to 15 and started 2026-08-26T20:46:21+0100. The merge was verified at 672 rows over 672 distinct keys, with no duplicate keys and the union exact.

## Censoring, read before the endpoints

`PLAN.md` section 8 named one-sided censoring as the hazard that would make the primary unreadable, and required `timed_out` to be read per arm first. It reads 1 run in the weighted arm and 0 in the control. `compare_timed_out` is 0 in both arms.

The single case is `prioritized-arbiter-aurus` at seed 15, weighted, capped at 3300 s with `implies_ideal = 0`, paired against a control run that scored 1. The censoring therefore falls exactly the way the threat predicted, on the arm under test, over one pair of 336. It accounts for 1 of the control's 19 discordant wins, and dropping it leaves 18 against 15, which does not move the verdict.

## The archived control as a validity check

Section 8 held the `monoon` rows of `2026-08-23-monotone` to be a check on the fresh control arm. Paired on all 336 `(spec, seed)` cases, the archive reads `implies_ideal` 0.6042 against the fresh 0.6071, agreeing on 335 of 336 individual runs (99.7%), with 0 archived-only wins and 1 fresh-only. `wall_time_s` reads 85.42 s against 88.45 s for a ratio of 1.036, and `n_repairs` 30.39 against 30.42 for a ratio of 1.001.

That licenses two conclusions and no more. It independently confirms the C++ trace in `PLAN.md` section 4, the archive having run the 0.33 triple where this campaign runs 0.1 / 0.2 / 0.7 while the control arm stays indistinguishable across the two, so the fitness weights do not reach the NSGA-II arm. And it shows the engine did not move between `6b78709` and `921ce1b` in anything this corpus detects.

## Endpoints, over 336 pairs

| endpoint | `nsga2-apportion` | `weighted` | ratio |
|---|---|---|---|
| `implies_ideal` | 0.6071 | 0.5952 | 0.980 |
| `found_repair` | 0.9970 | 0.9970 | 1.000 |
| `n_repairs` | 30.42 | 51.56 | 1.695 |
| `wall_time_s` | 88.45 | 236.39 | 2.673 |
| `n_implies` | 2.64 | 2.95 | 1.117 |
| `best_fitness` | 0.97 | 0.97 | 0.998 |

The primary is per-run `implies_ideal` greater than zero, paired by `(spec, seed)` and read by an exact two-sided *McNemar test*. It reads both 185, neither 117, control-only 19 and weighted-only 15, at p = 0.607591. The secondary `found_repair` reads both 334, neither 0, and one discordant pair each way, at p = 1.000000. The primary is null.

## Cost, the one separation

`weighted` is slower on 21 of 21 families, an exact two-sided sign test at p = 0.000001. The per-family wall ratio runs from a minimum of 1.03x through a median of 2.35x to a maximum of 3.87x, and pools to 2.673x over the 336 pairs.

## Per family

Sixteen pairs each. The discordant columns count pairs won by that arm alone; wall times are per-run means in seconds.

| family | control | weighted | control only | weighted only | control s | weighted s | ratio |
|---|---|---|---|---|---|---|---|
| `prioritized-arbiter-aurus` | 0.88 | 0.75 | 2 | 0 | 350.8 | 1331.3 | 3.80 |
| `lift` | 0.00 | 0.00 | 0 | 0 | 299.8 | 1061.1 | 3.54 |
| `round-robin-arbiter-aurus` | 1.00 | 0.94 | 1 | 0 | 137.5 | 451.2 | 3.28 |
| `humanoid-458` | 0.00 | 0.00 | 0 | 0 | 186.6 | 329.7 | 1.77 |
| `gyro-var1` | 0.00 | 0.00 | 0 | 0 | 136.8 | 267.7 | 1.96 |
| `gyro-var2` | 0.19 | 0.00 | 3 | 0 | 111.2 | 261.0 | 2.35 |
| `full-arbiter-aurus` | 0.75 | 0.69 | 3 | 2 | 201.9 | 208.1 | 1.03 |
| `minepump` | 0.81 | 0.62 | 5 | 2 | 41.1 | 142.1 | 3.46 |
| `lily11` | 0.31 | 0.75 | 1 | 8 | 36.0 | 121.4 | 3.37 |
| `simple-arbiter-aurus` | 1.00 | 1.00 | 0 | 0 | 31.1 | 120.3 | 3.87 |
| `ltl2dba-theta-2` | 1.00 | 1.00 | 0 | 0 | 78.0 | 100.1 | 1.28 |
| `load-balancer-aurus` | 1.00 | 1.00 | 0 | 0 | 59.3 | 98.6 | 1.66 |
| `ltl2dba-r-2` | 1.00 | 1.00 | 0 | 0 | 33.8 | 94.6 | 2.80 |
| `arbiter-aurus` | 0.69 | 0.69 | 2 | 2 | 30.1 | 88.5 | 2.94 |
| `rg1` | 0.00 | 0.00 | 0 | 0 | 21.9 | 78.6 | 3.59 |
| `lily02` | 1.00 | 1.00 | 0 | 0 | 18.5 | 51.4 | 2.77 |
| `detector-aurus` | 1.00 | 1.00 | 0 | 0 | 18.8 | 41.9 | 2.23 |
| `lily15` | 0.12 | 0.00 | 2 | 0 | 15.8 | 33.1 | 2.10 |
| `ltl2dba27` | 1.00 | 1.00 | 0 | 0 | 16.1 | 32.2 | 2.00 |
| `rg2` | 1.00 | 1.00 | 0 | 0 | 16.9 | 31.8 | 1.88 |
| `lily16` | 0.00 | 0.06 | 0 | 1 | 15.5 | 19.4 | 1.25 |

Nine families carry any discordance at all. Their exact McNemar values are `lily11` 0.0391, `gyro-var2` 0.2500, `minepump` 0.4531, `lily15` 0.5000, `prioritized-arbiter-aurus` 0.5000, and `arbiter-aurus`, `full-arbiter-aurus`, `lily16` and `round-robin-arbiter-aurus` at 1.0000. Bonferroni over the 21 families puts `lily11` at 0.820 and every other corrected value at 1.000.

`lily11` is a lead. The design licensed testing all 21 families, and one uncorrected p of 0.0391 among 21 tests is what the null produces; the 8 weighted-only pairs against 1 the other way are what make it worth a second look rather than a claim.

## Mechanism, from the per-run manifests

The extra wall time is `ltlfilt` time. `ltlsynt` barely moves. Three families carry the pattern, control arm first.

- `ltl2dba27`: `ltlfilt` calls 3999.56 → 8514.31 (2.13x), `ltlfilt` total_s 50.86 → 114.43 (2.25x), `n_accumulated_repairs` 82.12 → 245.38 (2.99x), `n_spot_decided` 2168.44 → 4216.19 (1.94x), `wall_s` 16.04 → 32.19 (2.01x).
- `simple-arbiter-aurus`: `ltlfilt` calls 4841.69 → 18217.50 (3.76x), `ltlfilt` total_s 94.85 → 447.54 (4.72x), `ltlsynt` calls 1640.38 → 1995.38 (1.22x), `ltlsynt` total_s 13.94 → 16.34 (1.17x), `n_accumulated_repairs` 59.69 → 233.75 (3.92x), `n_spot_decided` 2898.38 → 14006.81 (4.83x), `wall_s` 31.07 → 120.30 (3.87x).
- `rg1`: `ltlfilt` calls 2981.88 → 12014.12 (4.03x), `ltlfilt` total_s 54.24 → 277.23 (5.11x), `ltlsynt` calls 1766.38 → 2037.94 (1.15x), `n_accumulated_repairs` 27.31 → 140.94 (5.16x), `n_spot_decided` 1862.50 → 8475.12 (4.55x), `wall_s` 21.84 → 78.59 (3.60x).

The `ltlfilt` seconds track the wall ratio closely on all three while the `ltlsynt` seconds stay flat. What follows is a reading of those counters and not a measurement. At a status weight of 0.7 the weighted arm ranks chiefly by the status objective, so far more of its candidates clear the output gate and accumulate, which `n_accumulated_repairs` shows at 2.99x to 5.16x, and each distinct candidate costs one `ltlfilt` exec on a cache miss.

The consequence to state plainly is that the cost finding is entangled with the chosen weights. It need not hold at the shipped 0.2 / 0.5 / 0.5, nor at the 0.33 triple every archived campaign ran. Corpus-level evidence also says the output stage is not the whole of it: over the 336 pairs the correlation between the per-pair change in `n_repairs` and the change in `wall_time_s` is 0.191, and `ltl2dba27` runs 2.00x slower while emitting slightly fewer repairs, 24.2 against 23.2.

## Against the pre-registration

Section 7 registered no decision rule and stated what the campaign could and could not license. It can say that the 2026-07-24 quality gap does not survive. That ablation measured `weighted` at 14.9% against `nsga2-truncate`'s 23.7% on TLSF, roughly 2 to 1, and here the two arms are level at 0.5952 against 0.6071 with p = 0.607591.

The ablation's yield finding, `weighted` at 87.3% against 76.4%, is not testable on this corpus. Both arms sit at 0.9970 on `found_repair`, which section 6 predicted in advance from the control arm's per-family rates in the monotone archive.

Cost is the unregistered result, and the only separation the campaign found. Nothing here supports moving the default: `nsga2-apportion` is level on both endpoints and 2.673x cheaper. That is what the evidence supports and not what the pre-registered endpoint says, the primary being null at p = 0.607591 and a null endpoint carrying no direction of its own.

## Limitations

- **TLSF only.** No FRETISH phase runs, so the 2026-07-24 FRETISH finding is neither replicated nor contradicted.
- **The corpus is cost-selected.** Four families were excluded on measured cost alone (`PLAN.md` section 5), three of which sat at `implies_ideal` 0.10 under the control arm, so the excluded set is nearly all headroom this campaign cannot see.
- **One weight setting.** The weighted arm is measured at 0.1 / 0.2 / 0.7 alone, and the mechanism reading above is a reason to expect its cost to depend on that.
- **336 pairs resolve roughly 0.10 absolute.** A quality difference smaller than that is out of reach here, so the null on `implies_ideal` bounds the gap rather than closing it.
- **One censored run, one-sided.** The censoring section above gives the case and its effect on the discordant counts.

## What is owed

A weights sweep would separate the cost finding from the weight choice, and it is the natural follow-up, since the mechanism reading predicts the gap narrows as the status weight falls. `lily11` deserves a targeted look at more seeds. Neither is a commitment.

The campaign was declared a smoke test and it behaved like one: the endpoint it registered came back null, and the figure worth carrying forward, a 2.673x wall-time ratio, was never registered at all. Writing the weights and the exclusions down first is what keeps that ratio attached to one configuration, where a later sweep can move it.
