# Sweep U: assumption construction, and three outcomes that all miss

Six arms over 23 *Temporal Logic Synthesis Format* (TLSF) families at seeds 0 to 11, 1,646 rows. Sweep U crosses an operator factor against a search-size factor: `reachoff` holds all five new keys at their no-op values, `reach` arms the four assumption-construction keys, `reachburst` is `reach` plus `p_burst_continue = 0.5`, and the `-s` and `-l` suffixes are gen10/pop200 against gen40/pop400. The five keys are `max_assumption_width`, `p_bare_assumption`, `p_remove_assumption`, `p_union_assumption` and `p_burst_continue`. Every arm runs at `nsga2-apportion` with the log similarity metric, the weakening screen off, `accumulate_repairs = true`, `p_monotone` and `p_clone_assumption` at 0.25, `elitism_rate = 0.1`, a 7200 s wall cap per specification, a `compare_timeout` of 1800 s and `jobs = 8`. `PLAN.md` beside this file pre-registered the arms, the primary contrast, the regression gate and the three outcomes of section 5, before any row existed.

Every figure below reproduces with `python3 experiments/2026-08-26-assumption-reach/scripts/analyse_assumption_reach.py --csv experiments/2026-08-26-assumption-reach/results-assumption-reach.csv --complete-cells`, reading the merged CSV that sits in this directory. Both sides of every contrast are arms of this campaign at one binary, which is what the monotone campaign could not say of its control.

Provenance is `2f2e260` on branch `feat/assumption-construction`. Rows carry two binary commits, `0510b31` for seeds 0 to 7 and `8b3cb23` for the top-up seeds 8 to 11; the diff between them touches `PLAN.md`, `campaign.toml` and `scripts/run_experiments.py` and no file under `src/`, `include/` or `test/`, so both halves came from an identical binary. All 1,646 rows record `dirty=0`. av2 (`avlab12`) held seeds 0 to 3 and 8 to 9 for 820 rows, av3 held seeds 4 to 7 and 10 to 11 for 826.

10 of the 1,656 planned cells were not run. All ten are large-arm runs of `pcar-v2-888`, `prioritized-arbiter-aurus` and `round-robin-arbiter-aurus`, and the campaign was closed with them in flight. All three `-s` arms are complete at 276 of 276, so the primary, the regression gate and the burst contrast are final. Every contrast below is taken over the 271 `(spec, seed)` cases present in all six arms.

## The contrasts

The read is an exact two-sided *McNemar test* on per-run `implies_ideal`, scored by `compare` against `examples/<spec>/fixes`.

| contrast | scope | before → after | gained / lost | p |
|---|---|---|---|---|
| `reachoff-s` → `reach-s` (primary) | 3 target families, 36 pairs | 1 → 1 | 1 / 1 | 1.0000 |
| `reachoff-s` → `reach-s` (gate) | other 20 families, 235 pairs | 149 → 145 | 15 / 19 | 0.6076 |
| `reach-s` → `reachburst-s` | 271 pairs | 146 → 130 | 13 / 29 | 0.0195 |
| `reach-s` → `reachburst-s` | 3 target families, 36 pairs | 1 → 4 | 3 / 0 | 0.2500 |
| `reachoff-s` → `reachoff-l` | 271 pairs | 150 → 154 | 22 / 18 | 0.6358 |
| `reach-l` → `reachburst-l` | 271 pairs | 152 → 154 | 10 / 8 | 0.8145 |
| `reachoff-l` → `reach-l` | 3 target families, 36 pairs | 7 → 6 | 5 / 6 | 1.0000 |
| `reachoff-l` → `reach-l` | 271 pairs | 154 → 152 | 8 / 10 | 0.8145 |

The primary has two discordant pairs, which puts the exact two-sided floor at 2/2^2 = 0.5000. The burst on the target families has three, for a floor of 0.2500, so its 3 gained against 0 lost cannot reach 0.05 whatever the signs do. One contrast separates: the burst, pooled, and it separates downwards.

## None of the three outcomes fires

Section 5 registered three outcomes on the primary and exactly one was to be taken. None of them applies to what the campaign produced. Outcome 1 needs `reach-s` higher at p < 0.05, and the primary reads p = 1.0000. Outcome 2 needs `reach-s` to repair at least one target run and `reachoff-s` none, and `reachoff-s` repairs `gyro-var2` at seed 7. Outcome 3 needs `reach-s` to repair no target run, and it repairs `gyro-var2` at seed 2.

This is a defect in the registration rather than an ambiguity in the data. The three outcomes partition the space only if the control is pinned at zero on all three target families, and section 2 of the same plan records `gyro-var2` at 8 of 60 across the monotone campaign's three arms. That figure was in front of the author when the rule was written. A control that repairs a target run at 1 of 36 falls between outcome 2 and outcome 3, and the rule says nothing about it.

The numbers themselves are unambiguous. The target rate is 1 of 36 either way, the one success on each side is `gyro-var2` at a different seed, and the regression gate does not fire at 149 to 145 with p = 0.6076.

## The decision

All five keys ship at their no-op values: `max_assumption_width = 1`, `p_bare_assumption = 0.0`, `p_remove_assumption = 0.0`, `p_union_assumption = 0.0` and `p_burst_continue = 0.0`. That is outcome 3's action, reached by reading the numbers rather than by the rule firing, and it is recorded here as such. `p_burst_continue` goes off with a measured pooled loss behind it, 13 gained against 29 lost at p = 0.0195, rather than with an argument.

The operators stay in the tree at those values. Each key is tested before the `RandomSource` is touched, so an unarmed key costs no draw and the shipping breeding stream is byte-identical to the one before the five existed; `test/tlsf/assumption_tests.cpp` and the draw-count golden in `test/tlsf/monotone_tests.cpp` pin that. Two reachability results are worth keeping the code for, and both are below.

**No config-vintage entry is owed**, which supersedes section 10's expectation of five. All five keys are new and all five default to a no-op, so an archived config that omits them means exactly what it meant before they arrived. A vintage entry is for a default that moved.

## Why lift moved and gyro did not

`lift` was repaired for the first time in the project's history. It read 0 of 60 across all three arms of `experiments/2026-08-23-monotone`, and here it reads 3 of 12 at `reach-l` against 0 of 12 for its own control, 3 gained and none lost. Its ideal is `G F (b1 || b2 || b3)`, a width-3 disjunction, which is the shape `max_assumption_width = 3` was written to emit. Section 6 named `lift` as reachable only through that key before any row existed, so the attribution is registered rather than found afterwards.

`lily11` is the second such case, at 5 of 12 to 11 of 12 on the primary contrast. Section 6 named it as reachable only through `p_bare_assumption`, its whole ideal being a bare `F req`, against which `G F req` is strictly stronger. Neither family is a target family, so neither bears on the primary.

`gyro-var1` is unmoved at 0 of 72 across all six arms. `gyro-var2`'s gains track search size instead of the operators: it reads 7 of 12 at `reachoff-l` against 3 of 12 at `reach-l`, so the no-op control leads its armed counterpart at the larger budget.

The mechanism behind that is worth stating, because it is why `p_union_assumption` could not deliver what it was written for. `gyro-var2`'s ideal is a roughly 29-node conjunct that is verbatim a conjunct of *`gyro-var1`'s specification*, and `gyro-var1`'s ideal is two conjuncts of `gyro-var2`'s. The union operator takes a whole live ASSUME conjunct from the second parent, and both parents in any crossover descend from the same original within one run. The material each of these two families needs sits in a sibling specification that no run ever sees.

## The burst, and search size

`p_burst_continue` applies `1 + Geometric(p)` edits rather than one. Pooled over 271 cases it loses 29 runs and gains 13 at p = 0.0195, the campaign's one significant result. The losses concentrate in families that were already solved: `arbiter-aurus` falls from 8 of 12 to 2 of 12 at small size, `load-balancer-aurus` from 12 to 8, `rg2` from 12 to 9. At the larger budget the same contrast is null at 152 to 154 with p = 0.8145, so the damage is a small-budget effect.

Search size buys nothing measurable. `reachoff-s` to `reachoff-l` is 40 generations against 10 and 400 population against 200, and it moves `implies_ideal` from 150 to 154 of 271 at p = 0.6358 for a median wall time of 357.2 s against 39.0 s, roughly 9x. Yield falls from 99.6% to 84.1% over the same pair, with 15.9% of the large arm's runs hitting the 7200 s cap against none of the small arm's, and the median repair count rises from 28.0 to 78.0. `full-arbiter-aurus` is the sharpest case, reading 10, 5 and 5 of 12 across the three small arms against 0, 1 and 3 across the three large ones.

## Secondary measures, none gating

| arm | yield | `implies_ideal` | timeout | median wall (s) | median `n_repairs` |
|---|---|---|---|---|---|
| `reachoff-s` | 0.996 | 0.554 | 0.000 | 39.0 | 28.0 |
| `reach-s` | 0.996 | 0.539 | 0.000 | 38.8 | 28.0 |
| `reachburst-s` | 0.996 | 0.480 | 0.000 | 38.9 | 27.0 |
| `reachoff-l` | 0.841 | 0.568 | 0.159 | 357.2 | 78.0 |
| `reach-l` | 0.845 | 0.561 | 0.155 | 333.2 | 75.0 |
| `reachburst-l` | 0.845 | 0.568 | 0.155 | 385.8 | 80.0 |

The four assumption-construction keys cost nothing in wall time, 38.8 s against 39.0 s at small size. The burst costs 385.8 s against 333.2 s at large size, 15.8% more, for the same 0.568 rate the no-op arm reaches.

## Threats and caveats

- **The four keys never vary independently.** `reach` turns all four on together, so nothing here attributes anything to one of them alone. Section 6 registered the bundling knowingly, and the per-family reads on `lift` and `lily11` carry information the sweep does not.
- **10 of 1,656 cells were not run.** All ten are large-arm cells of three families that saturate the wall cap. The primary, the gate and the burst contrast are unaffected, all three `-s` arms being complete.
- **The corpus is 23 families rather than the 25 of `H2H_TLSF_READY`.** `humanoid-531` and `humanoid-742` were excluded before launch as time-limited rather than grammar-limited; section 4 records the cost basis. Anything reading across this archive and the monotone one reads over 23 families on this side.
- **Pairing is on the case, not the trajectory.** The two arms of a contrast share `(spec, seed)`, and their `RandomSource` streams diverge from the first differing draw.
- **The large arms change two things at once.** `-l` moves generations from 10 to 40 and population from 200 to 400, so the search factor is a capacity contrast.
- **`implies_ideal` is scored against counter's curated ideal set**, identically for every arm, so it bears on absolute rates rather than on the paired differences.

## What is owed

The 2^4 that separates `max_assumption_width`, `p_bare_assumption`, `p_remove_assumption` and `p_union_assumption`. Section 6 registered the bundle before the run and the per-family attributions on `lift` and `lily11` are descriptive, so nothing in this archive disposes of any one key on its own.

The `p_monotone` tuning campaign, owed before this one and now with a measurement behind it. counter's `best_relation` reads `equivalent` on 1.1% of runs against AuRUS's 33.8%, and `rules_at` in `src/tlsf/mutation.cpp` gives an `Atom` only the `Constant` weakening where AuRUS's `FormulaWeakening` applies `a -> a|b` at a literal. That is a concrete gap in the rule table rather than a probability to tune.

Why 9x the search budget buys nothing. 40 generations against 10 and 400 population against 200 move the pooled rate from 150 to 154 of 271, and every grammar change proposed on this corpus runs into that result before it runs into anything else.

Two families are reachable that were not, and the registered target families are exactly the ones that stayed put. The plan that named them also named the shapes each key was written for, so what this campaign confirms and what it fails to confirm can be read apart, which is more than a pooled endpoint at p = 1.0000 would otherwise support.
