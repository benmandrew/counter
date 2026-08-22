# counter at the shipping configuration against AuRUS, at full power

One arm, closed 2026-08-22. It runs counter at its 2026-08-21 shipping configuration plus `accumulate_repairs = true`, over the 25 *Temporal Logic Synthesis Format* (TLSF) families of `H2H_TLSF_READY` at seeds 0 to 19, under a 7200 s wall cap, a `compare_timeout` of 1800 s and `jobs = 8`. The control is the AuRUS arm archived by `experiments/2026-08-14-aurus-h2h`: the same 780 repeat rows, unchanged, read by the same `scripts/analyse_aurus_h2h.py`. `PLAN.md` beside this file pre-registered the design, the decision rule of section 5 and the endpoint hazard of section 6 before any row of the campaign existed.

The campaign exists because two earlier readings each fell short of the question. `experiments/2026-08-14-aurus-h2h` measured a counter carrying none of the four changes since made permanent and found AuRUS higher at p = 0.0127 over 25 families. `experiments/2026-08-20-ops-weakening/REPORT.md` could read those changes against the AuRUS reference only over the 10 families the sweep-O corpus shares with it, where the exact two-sided p floor is 0.0156 over 7 clusters and every arm read null, the archived arm included at p = 0.1562. That restriction was a power loss rather than a result, and the subset is fair on effect size: mean gap 0.222 over the included 10 against 0.224 over the excluded 15. This campaign runs the full 25 at full power.

Provenance is `3bb28dc` on branch `campaign/aurus-h2h-ship`, launched through `scripts/campaign.py enqueue`. Both hosts reported `3bb28dc` from `counter --version` and neither was built dirty. av3 held seeds 10 to 19 and ran 2026-08-21T21:40:01+0100 to 2026-08-22T03:43:41; av2 held seeds 0 to 9 and ran 21:55:01 to 03:39:24. Each host wrote 250 rows, merging to 500 with 500 distinct keys and no duplicates. Every figure below reproduces with `python3 scripts/analyse_aurus_h2h.py experiments/2026-08-21-aurus-h2h-ship`, the merged CSV and the two AuRUS validation files sitting in that directory.

## Result

| | counter mean rate | AuRUS mean rate | families counter/AuRUS/tied | Wilcoxon p (family) | p (cluster) |
|---|---|---|---|---|---|
| 2026-08-14 archive | 0.281 | 0.504 | 5 / 14 / 6 | 0.0127 (W+ = 34.5, n = 19) | 0.0195 (W+ = 3, n = 9) |
| this campaign | 0.502 | 0.504 | 10 / 7 / 8 | 0.7549 (W+ = 83.5, n = 17) | 0.5898 (W+ = 17.5, n = 9) |

**Outcome 3 under section 5: no separation.** The 0.222 gap between the two mean rates is now 0.002, and the exact two-sided *Wilcoxon signed-rank test* reads 0.7549 per family and 0.5898 per cluster. The 2026-08-14 result does not reproduce at the shipping configuration. This is a tie. A p of 0.7549 does not license reporting counter as ahead, and section 5 fixes the outcome as that sentence and no more.

The hazard section 6 registered fired. Ties rose from 6 families to 8 and n fell from 19 to 17, because counter improved onto families where AuRUS already sat at the ceiling, and the Wilcoxon drops every tied family from the test. A better arm therefore presents a smaller n than the worse one it replaces. Section 6 required the sign counts, the tie count and both mean rates to sit beside the p value in every outcome, which is what the table above does; outcome 3 is read against those columns.

## Per-family detail

counter leads on 10 families, loses on 7 and ties on 8, against 5, 14 and 6 in the archive. Four of the seven losses carry the deficit.

| family | counter | AuRUS | diff |
|---|---|---|---|
| gyro-var2 | 0.100 | 0.700 | −0.600 |
| lily11 | 0.250 | 0.833 | −0.583 |
| lift | 0.000 | 0.300 | −0.300 |
| minepump | 0.750 | 1.000 | −0.250 |

The largest counter leads are `full-arbiter-aurus` at 0.550 against 0.000 and `prioritized-arbiter-aurus` at 0.500 against 0.000, both families where AuRUS emits nothing at all. Clustered, counter leads 5 clusters to 4 with 1 tied: `arbiter` at +0.175 over its 6 families, `pcar-v2-888` at +0.250, `ltl2dba` at +0.033, `rg` at +0.025 and `humanoid` at +0.025, against `gyro` at −0.333, `lift` at −0.300, `minepump` at −0.250 and `lily` at −0.096. Section 10.2 of the head-to-head plan gives the clustered test precedence where the two disagree, and here they agree.

`minepump` is worth naming for its size rather than its sign. It reads 0.750 here against 1 of 20 in the archived counter rows, and it remains a loss against AuRUS's 1.000. No per-family test was registered, so that reading is exploratory.

## The four losses, and what runs next

`gyro-var2` and `lift` are the two families `feat/monotone-operators` was written for. `tlsf_add_assumption` builds from a template emitting at most seven nodes, and `gyro-var2`'s sole ideal is a 29-node assumption, so the operator cannot express the target whatever the search does with it. The follow-up campaign `experiments/monotone`, on branch `campaign/monotone` at commit `6b78709`, runs three arms over this same corpus, these same seeds, the same 7200 s cap and the same host split, with these 500 rows as its outer control; its plan is `experiments/monotone/PLAN.md`. It had not finished when this report was written and no result for it is reported here.

## The paired counter-against-counter read

Over the 499 `(spec, seed)` cases both campaigns ran, `implies_ideal` was gained by this campaign on 125 and lost on 15, an exact two-sided *McNemar test* at p = 8.9e-23. That is the internal measure of what the four changes bought, and it is a secondary measure under section 7 rather than the registered endpoint, which is counter against AuRUS.

The two readings differ in character, not only in strength. The endpoint compares two rates against a fixed external reference and aggregates to 25 family numbers, so a family already at AuRUS's ceiling absorbs any gain counter makes on it and contributes a tie. The paired read compares runs against counter's own past at case granularity, where every one of those gains counts. A large within-counter move and a flat between-tool test are consistent, and the tie count of 8 is where the two views meet.

## Secondary measures, none gating

Yield is 466 of 500 (93.2%) against 447 of 499 (89.6%) in the archive. The timeout rate at the 7200 s cap is 34 of 500 (6.8%) against 19 of 499 (3.8%). Median wall time is 64.7 s against 21.6 s and mean 763.1 s against 427.1 s, for 106.0 machine-hours against 59.2. Median `n_repairs` is 32 against 4, with a maximum of 109 against 44. `compare` timed out 0 times against 1 in the archive, the 1800 s budget having been raised from 600 s for that reason, and it held.

One correction to the record. Section 7 of the plan states the archived arm timed out 0 of 499 runs on these families; counting the archived rows gives 19 of 499. The rows are what the analysis script reads, and the plan's figure is wrong.

The AuRUS side is unchanged from the archive and re-reported for completeness. It returns a median of 424 claimed repairs per run, 240 after the section 10.1 well-separation filter. Of 287,006 claimed repairs, 286,200 re-validate as realizable under `ltlsynt`, leaving 806 disagreements, and 113,283 (39.47%) are ill-separated with 0 undecided. The filter moves 2 of 25 family rates, `lift` from 0.167 to 0.300 and `lily11` from 0.900 to 0.833, and the unfiltered per-family test gives p = 0.7189 at n = 17. The verdict does not turn on the filter.

## Threats and caveats

- **The AuRUS rows are 30 repeats per family against counter's 20 seeds.** That is the archive's design, unchanged here so the comparison stays like-for-like with 2026-08-14.
- **AuRUS scored 26 families to counter's 25.** `humanoid-741` is AuRUS-only and is excluded from every test.
- **Host load is uncontrolled** and the two campaigns ran a week apart on the same two machines. The endpoint compares rates rather than runtimes, so this bears on the secondary timings alone.
- **The wall-clock and timeout rises are real** and attributable to the accumulator plus the weakening screen being off, both of which push more candidates through `compare`.
- **`run_well_separation` took the binary default of `false` here, deliberately.** `--pin-vintage` was not passed because it writes that key from a `gen_configs.DEFAULTS` entry stale since `b101ada`. The 2026-08-14 arm ran with the per-generation well-separation filter on, against its own profile's stated intent of tracking the shipping defaults. That is a real difference between the two counter arms, on top of the four changes the campaign is a bundle test of.

A campaign that closes on a tie is easy to misreport, since every per-family number in it can be quoted in either direction. The pre-registered rule is what fixes the reading, and the sign counts, the tie count and the two mean rates are recorded beside the p value so a later reader can check that it was applied.
