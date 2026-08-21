# The repaired grammar, with the weakening screen and without it

Two paired campaigns closed on 2026-08-20 and disagree. Both run sweep O, whose arms are `opslegacy` — the *mutation and crossover grammar* counter shipped before 2026-08-19 — and `opsfixed`, the same grammar with the seven defects from `experiments/2026-08-14-aurus-h2h/REPORT.md` repaired behind `[genetic] repaired_operators`. Both pair on `(spec, seed)` with both arms of a pair on one host, and both take the paired per-run `implies_ideal` as the primary endpoint under an exact McNemar test, computed per path and never pooled. `experiments/2026-08-20-ops-grammar/PLAN.md` pre-registered the design, the decision rule of section 9 and the wall-ratio amendment of section 9a before any endpoint row was collected.

One setting separates them. `ops-grammar` ran with the *weakening screen* on (`run_weakening = true`), the final filter that keeps only genuine weakenings of the original, which is what every campaign archived under `experiments/` ran. `ops-weakening` ran the identical cross with the screen off, the default counter was moving to, and section 8 of the plan registered it in advance as "the one that decides what ships".

## Results

| campaign | path | pairs | `opsfixed`-only | `opslegacy`-only | p | median paired wall ratio | rate |
|---|---|---|---|---|---|---|---|
| `ops-grammar` | FRETISH | 120 | 9 | 3 | 0.146 | 1.699 | 66.7% → 71.7% |
| `ops-grammar` | TLSF | 240 | 25 | 12 | 0.047 | 1.230 | 33.3% → 38.8% |
| `ops-weakening` | FRETISH | 120 | 5 | 4 | 1.000 | 1.714 | 74.2% → 75.0% |
| `ops-weakening` | TLSF | 240 | 15 | 15 | 1.000 | 1.229 | 41.2% → 41.2% |

Neither campaign timed out a single run on either arm of either path, so the directional cap censoring section 8 registered as the leading threat did not occur. Yield with the screen off is 100% on both arms and both paths, against 96.7–98.8% under it.

`ops-grammar` therefore fires outcome 1. The *Temporal Logic Synthesis Format* (TLSF) path wins its own endpoint at p = 0.047, neither path loses significantly, and that path holds its own median paired wall ratio at 1.230 under the 1.25 bound amendment 9a fixed as per path. `ops-weakening` fires outcome 3, no significant difference either way.

## The 2×2

Both campaigns ran the same `(spec, seed)` cases, 120 FRETISH and 240 TLSF, so the four cells assemble directly.

| path | arm | screen on | screen off |
|---|---|---|---|
| TLSF (of 240) | `opslegacy` | 80 | 99 |
| TLSF (of 240) | `opsfixed` | 93 | 99 |
| FRETISH (of 120) | `opslegacy` | 80 | 89 |
| FRETISH (of 120) | `opsfixed` | 86 | 90 |

No run anywhere lost `implies_ideal` when the screen came off, 0 of 720. The gains are 19 of 240 TLSF runs under `opslegacy` and 6 under `opsfixed`, 9 of 120 FRETISH runs under `opslegacy` and 4 under `opsfixed`. That direction is guaranteed by the design rather than discovered by the measurement: `run_weakening_filter` is a final screen over the realizable survivors (step 6 of the algorithm flow), it draws nothing from the `RandomSource`, and the two campaigns' matched runs therefore search identically, differing only in what survives to output. `best_fitness` bears that out, `parse_repair_files()` deriving it from the emitted repair files rather than from the population, so its moving between the campaigns is what a final screen does. A two-sided McNemar is the wrong instrument here — its null of "the screen discards nothing that matters" predicts 0 discordant pairs, and any real effect must appear one-way — so the counts carry the result and a p-value on them would overstate it.

The screen-off set is not a strict superset of the screen-on one. `n_repairs` fell in 27 of the 720 matched runs, 22 TLSF and 5 FRETISH. The implication filter runs after the weakening screen and keeps only maximal specs, so admitting more candidates lets a non-weakening dominate and displace several weakenings, shrinking the antichain. Losing `implies_ideal` is possible for that reason and did not happen.

Read along the other axis, the grammar effect is +13 TLSF runs with the screen on and 0 with it off, and +6 FRETISH runs on against +1 off. The two changes recover substantially the same repairs. Section 8 pre-registered this asymmetry as a threat and expected it to favour the treatment arm; it ran the other way.

No code change confounds the weakening contrast. `201d925..09a5eb8` touches only `scripts/` and `experiments/`, nothing under `src/`, `include/`, `cmake/` or `CMakeLists.txt`, and across a matched pair of runs exactly one of the 41 effective config keys in `run.json` differs, `filters.run_weakening`. The contrast still runs between two campaigns rather than within one, which is what keeps it descriptive.

## Beneath the pooled null

The plan registered no per-spec test, so everything in this section is exploratory.

The TLSF 15-15 is heterogeneous rather than flat. Per spec at 20 seeds each, `implies_ideal` hits read (`opslegacy` on, `opsfixed` on, `opslegacy` off, `opsfixed` off): `minepump` 0, 12, 7, 13; `codesample-un2` 16, 20, 18, 20; `humanoid-531` 11, 6, 12, 6; `lily02` 14, 15, 20, 20; `arbiter-aurus` 18, 19, 19, 19; `rg2` 20 in all four cells. `codesample-un1`, `gyro-var1`, `gyro-var2`, `humanoid-458` and `lift` sit at or near 0 in all four. On FRETISH, `fsm` reads 18, 26, 26, 28 with 9-1 discordant pairs under the screen (p = 0.021), `fsm-combined` 2, 0, 3, 2, and `fsm-timing` and `takeoff` are 30 of 30 in all four cells.

**`minepump` is the sharpest case.** It is unreachable under the legacy grammar with the screen on at 0 of 20, and reaches 12 of 20 with the repairs (12-0 discordant, p = 0.0005). Dropping the screen recovers 7 of those 12 through the legacy arm, and the repaired grammar still leads 13 to 7 at a sample 20 seeds cannot resolve (8-2, p = 0.109).

**`humanoid-531` moves the other way,** losing runs in both campaigns at 3-8 and 3-9 discordant. It is the family `ops-pilot` measured building much larger formulae under the treatment arm, at a median of 456.5 s against 310.6 s and a maximum of 630.4 s against 390.4 s. The graft helps where a repair needs new material and hurts where it only bloats.

## The decision, and the divergence

On 2026-08-20 the maintainer made the repaired grammar permanent. `repaired_operators` was removed rather than re-defaulted, both legacy code paths deleted, and `run_weakening_filter` flipped to false in the same change.

That decision rests on `ops-grammar`'s outcome 1 firing on TLSF, on `minepump`'s reachability result, and on the 2026-08-14 audit of the operators themselves. It does not rest on the pooled primary endpoint under the shipping configuration, which is null on both paths. Read under the configuration section 8 named as decisive, the pre-registered rule gives outcome 3 and says do not flip. The divergence is recorded here.

The screen evidence is the largest the project has: 38 of 720 matched runs gain `implies_ideal` and none loses one, across both paths and both grammars, against `2026-08-19-weakening-arbiter`'s 9 of 120 paired repairs at p = 0.002. Its direction comes from the design rather than from the data, so those counts and that coverage are what the `run_weakening_filter` flip rests on.

Sweep O retires with the key, as do the `ops-*` and `opswk-*` runner profiles. Every config in both archives sets `repaired_operators`, on both arms, so a current binary rejects all of them; both campaigns reproduce only at the commits their `PROVENANCE.json` files name, through the vendored `scripts/` beside them.

A repair the search cannot reach and a repair the final screen discards look identical in a yield count, and the 2×2 is the only view assembled here in which the two come apart. Both archives keep their rows against the same frozen ideals, which is what a later reading of this decision will need.

## Against the AuRUS reference

A cross-campaign comparison ran on 2026-08-21, reading both sweep-O campaigns' TLSF rows against the AuRUS arm archived by `experiments/2026-08-14-aurus-h2h`. `scripts/analyse_aurus_h2h.py` computes it unchanged, one run per arm. No new code was needed. That script scores over the intersection of the two corpora, so restricting the family set is automatic, and its cluster loop skips a cluster no scored family belongs to, so it drops to 7 clusters unprompted. The test is the head-to-head plan's own rule — an exact *Wilcoxon signed-rank test* over families and over clusters, two-sided at alpha 0.05, AuRUS scored on `implies_genuine` over well-separated repairs per §10.1 and counter on `implies_ideal`.

Every row of the table below reproduces the same way. For each arm, filter the campaign's results CSV on `level_name` — `opsfixed` or `opslegacy` — write it into a scratch directory under the name `results-aurus-h2h.csv`, copy `validation-av2.csv` and `validation-av3.csv` from the head-to-head archive in beside it, and run `python3 scripts/analyse_aurus_h2h.py <that directory>`. The archived arm's own restricted row comes from the same recipe with `results-aurus-h2h.csv` filtered to the 10 shared families instead. The script warns that the cluster map covers families the scored set does not, which is the restriction reporting itself rather than a fault.

Every row now sits under `experiments/`. `results-aurus-h2h.csv` holds 499 rows, `results-ops-tlsf.csv` 480, `results-opswk-tlsf.csv` 480, `results-ops-fret.csv` 240 and `results-opswk-fret.csv` 240, each merged from av2 and av3 and verified as a union with no duplicate keys. The AuRUS arm's own files sit in the head-to-head archive, `validation-av2.csv` and `validation-av3.csv` carrying 780 repeat rows beside `aurus_results-av2.csv`, `aurus_results-av3.csv`, `wellsep-av2.csv` and `wellsep-av3.csv`. Running `analyse_aurus_h2h.py` over the collected files reproduces the archived decision exactly, at W+ = 34.5 and p = 0.0127 over 25 families, W+ = 3 and p = 0.0195 over 10 clusters, and mean rates of 0.281 for counter against 0.504 for AuRUS. The FRETISH rows are collected for completeness and take no part here, AuRUS being TLSF-only.

The sweep-O corpus is 12 families, and 10 of them have AuRUS rows. `codesample-un1` and `codesample-un2` are counter-only and drop out, and 16 AuRUS families have no sweep-O rows. The 10 that remain fall in 7 of the head-to-head's 10 clusters — arbiter, lily, humanoid, gyro, rg, lift and minepump — each represented by one or two members rather than all of them. At 7 clusters the exact two-sided p floor is 0.0156, reachable only with all 7 pointing one way. `analyse_aurus_h2h.py` now prints that floor at every unit count rather than only below 6, where it had been reporting outcome 3 by construction, so the bound is visible in any restricted read rather than left to be worked out. A null at this scope is not evidence of parity.

The restriction alone dissolves the archived verdict. Over the same 10 families, the archived head-to-head counter arm reads p = 0.1562 per family, with 2 families counter-higher, 5 AuRUS-higher and 3 tied, and p = 0.1562 clustered, at 2 against 4 with 1 tied. The 25-family verdict does not survive restriction to these 10, and none of that loss is attributable to the newer arms.

Per-family mean `implies_ideal` rate over the 10 runs from 0.265 on the archived arm to 0.400 on `opslegacy` with the screen off, against AuRUS at 0.487 on the same 10.

| arm | mean rate | p (family) | p (cluster) |
|---|---|---|---|
| AuRUS | 0.487 | — | — |
| `aurus-h2h` archived | 0.265 | 0.1562 | 0.1562 |
| `opslegacy`, screen on | 0.320 | 0.1953 | 0.1562 |
| `opsfixed`, screen on | 0.365 | 0.2109 | 0.2969 |
| `opslegacy`, screen off | 0.400 | 0.5469 | 0.6094 |
| `opsfixed`, screen off | 0.395 | 0.4844 | 0.5781 |

Per-family sign counts read 2 counter-higher, 5 AuRUS-higher and 3 tied on the archived arm, and 4, 4 and 2 in both screen-off arms; clustered, they move from 2, 4 and 1 to 4, 3 and 0. Every arm reads no difference.

**`minepump` carries the largest move.** Per family at 20 seeds each, rates read in the order archived, `opslegacy` on, `opsfixed` on, `opslegacy` off, `opsfixed` off. `minepump` runs 0.050, 0.000, 0.600, 0.350 and 0.650 against AuRUS at 1.000, the family named above as unreachable under the legacy grammar. `arbiter-aurus` goes from 0.500 to 0.950 and passes AuRUS's 0.933, and `lily02` reaches 1.000 in both screen-off arms against 0.867. `humanoid-531` moves the other way under the repairs, 0.550 to 0.300 with the screen on and 0.600 to 0.300 with it off, against AuRUS at 0.000. `gyro-var2` and `lift` read 0.000 in every counter arm against 0.700 and 0.300, and carry most of what remains of the gap. `gyro-var1` reads 0.000 against 0.067, `humanoid-458` and `rg1` are ties near zero, and `rg2` is 1.000 everywhere.

Four things separate the sweep-O arms from the archived counter arm beyond the three changes under test, none of them a choice either campaign made. All four are `gen_configs` baselines that deliberately do not track the binary default. `selection_scheme` is `nsga2-apportion` on the head-to-head and `nsga2-truncate` on the sweep-O arms, where the binary default is `Nsga2Apportion` at `config.hpp:290` and `2026-08-11-selection-default` measured the pair. `metric` is `logarithmic` against `direct`, where the default is `Logarithmic` at `config.hpp:109` and `2026-07-16-metric` crossed the two. `run_well_separation` is true against false, the default having gone false in `b101ada` on 2026-08-10 when the status score absorbed the property. The wall cap is 7200 s, matched to AuRUS, against 600 s, or 2580 s for `humanoid-531` and 960 s for `lift`.

AuRUS ran at 7200 s in both comparisons, so the cap difference reinstates the asymmetry the 2026-08-14 campaign existed to remove, in the direction that favours AuRUS. That asymmetry and the well-separation difference both push in directions this comparison cannot separate from the three changes under test.

Two corrections to the record follow from reading the configs. `ops-grammar/PLAN.md` §4 excludes the archived counter rows as a control, on the grounds that the head-to-head inherited a 500 ms `ltlsynt` budget from `ablate-tlsf`. The head-to-head config sets no such key. Its `gen_configs` invocation carried no `--tlsf`, so there was no generator default to inherit, and it ran at the binary default, which `74beaea` raised to 10000 ms on 2026-08-11 — the same value the sweep-O configs set explicitly. The synthesis budgets match, so that barrier is not real.

The `aurus-h2h` profile states that it tracks the shipping defaults, a head-to-head having to run counter as it ships. `--pin-vintage` takes its values from `gen_configs.DEFAULTS`, whose `run_well_separation` entry still reads true and has been stale since 2026-08-10. The head-to-head arm therefore ran the per-generation well-separation filter where the shipping binary does not, against that profile's stated intent.

The comparison supports two statements. The gap to AuRUS narrows across the arms on these 10 families and the direction is consistent, and it reaches significance in neither direction at this scope with these confounds. Settling it needs the sweep-O configuration re-run at `nsga2-apportion`, the logarithmic metric, well-separation off and the 7200 s cap, over the full 25-family head-to-head corpus.

Ten of 25 families is what the two corpora share on 2026-08-21, and the restriction moves the verdict before any arm does. The rows are archived beside the campaigns they read and the analysis is the head-to-head's own script, so the wider re-run has one configuration to change and nothing to reconstruct.
