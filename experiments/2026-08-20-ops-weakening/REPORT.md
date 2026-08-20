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
