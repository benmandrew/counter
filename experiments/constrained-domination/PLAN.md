# Constrained domination against plain Pareto domination: one key, 630 pairs

Pre-registered 2026-09-08, before any row of this campaign was run.

## 1. Question

Does ranking the NSGA-II population by Deb's constrained domination, with feasibility set at the top status tier, change repair quality against the plain Pareto ranking the search ships with?

`genetic.constrained_domination` is the only key that differs between the arms. Under it a candidate at the top status tier precedes one below it whatever the two similarity objectives say, two candidates below it are ordered by how far below they sit, and two at the tier fall through to ordinary dominance.

## 2. Why the archived rows do not answer it

The key did not exist until this branch, so no archived row carries either value of it. The campaigns that vary `selection_scheme` — `2026-07-24-ablation`, `2026-08-26-selection-smoke`, `2026-08-28-selection-grading`, `2026-08-29-aurus-matched` — all cross whole schemes rather than the ranking rule inside one, and every NSGA-II row in the archive is a plain-domination row.

What motivates the arm is a measurement rather than an argument. Both similarity objectives are computed against the original specification, so the original scores 1.0 on each by construction. Nothing dominates a point maximal in two of three objectives, and (μ+λ) pooling never discards it. Instrumenting the rank-0 front at generation 10 over 21 runs — `arbiter-aurus`, `fsm`, `fsm-combined`, `fsm-timing`, `lily11`, `minepump` and `rg2` at seeds 0, 1 and 2, under `nsga2-apportion` — found the original on the front in 21 of 21, at exactly (1.0000, 1.0000) on the two similarity axes. It holds a front slot and breeds every generation, in every family tried.

The smallest ε-dominance margin that would displace it runs from 0.0010 on `fsm-timing` to 0.3636 on `lily11`, a factor of 364, so no single margin covers the corpus. A constraint needs no margin, which is why this arm is the one being measured.

## 3. Design

One factor, two levels, paired. Sweep K's `cdoff` is the control and reproduces the shipping ranking; `cdon` is the treatment. 21 families × 30 seeds = 630 paired `(spec, seed)` cases, 1,260 runs. Seeds 0-14 on av2 and 15-29 on av3, so both arms of a pair run on the same host and a host difference cancels inside the pair.

## 4. Configuration

Everything but the one key is the shipping configuration. Both sweep levels pin `p_monotone = 0.25`, `p_clone_assumption = 0.25`, `elitism_rate = 0.1` and `accumulate_repairs = true`, the four keys sweep T's `monoon` level pins, at today's `include/config.hpp` values, so a row here is readable against `2026-08-26-selection-smoke` and `2026-08-23-monotone` rather than against whatever the binary defaulted to on the day. `--pin-vintage` writes the vintage keys explicitly for the same reason.

`--weights 0.2 0.5 0.5` is the binary's own triple rather than `gen_configs.py`'s 0.33/0.33/0.33, which is neither a default nor anything a user gets. Under `nsga2-apportion` the weights never enter selection, both arms ranking by domination, so the choice governs what `best_fitness` reports and nothing else.

`status_grading` stays at `mrs`, the shipping value. It matters here beyond being the default: feasibility is the top status tier, and the tiered scale would collapse the infeasible region to two values where MRS spreads it over the guarantee-part count, which is what gives the violation ordering anything to order.

## 5. Corpus

`SELECTION_SMOKE_SPECS`, the 21 families `H2H_TLSF_READY` holds once `humanoid-503`, `humanoid-531`, `humanoid-742` and `pcar-v2-888` are excluded on cost. That is selection-smoke's corpus unchanged, so this campaign's control arm is directly readable against that campaign's control arm. Those four families together are 84% of a seed's serial cost over the 25, so keeping them means the campaign does not finish rather than finishing slowly.

## 6. Endpoints and the decision rule

**Primary.** Per-run `implies_ideal > 0`, paired by `(spec, seed)`, two-sided exact McNemar over the discordant pairs, α = 0.05. 630 pairs against selection-smoke's control rate of 0.6071 resolves roughly 0.05 absolute; anything smaller is out of reach at this budget and this plan does not claim it.

**Decision rule, registered.** Adopt `constrained_domination = true` as a candidate default only if the primary is significant *and* directionally favourable. A significant result in the other direction retires the arm. A null retires it too: the argument for the constraint is that it removes a candidate that cannot be output, and if removing it buys nothing measurable then the front slot it occupies was not costing anything either.

**Secondary, no decision attached.** Yield (`found_repair`), `n_repairs`, `best_relation`, and `wall_time_s` per arm.

The treatment arm is the more expensive one, and that was measured before launch rather than assumed. A 12-case probe — `arbiter-aurus`, `detector-aurus`, `lily11`, `lily16`, `minepump` and `rg2` at seeds 0 and 1, both arms — reads a median paired wall ratio of **1.67** and a maximum of 2.71. `n_repairs` moves with it, 28 to 74 on `arbiter-aurus` seed 0 and 44 to 78 on `minepump` seed 1, which is the likely mechanism: the constraint keeps more distinct feasible candidates, and the implication filter is quadratic in that set and already dominates a run. Twelve cases on six families is a probe rather than a result, and this campaign measures the ratio properly.

**Read before the primary.** `timed_out` per arm. A one-sided kill rate makes `implies_ideal` uninterpretable, which is what `2026-08-28-selection-grading` cost before its re-score. The caps are set for this: selection-smoke's table, which is 4x the slowest run its `nsga2-apportion` arm recorded per family, scaled by 1.7 and clamped at 3600 s, so the treatment arm keeps roughly the margin the control arm had rather than 1.5x of it.

## 7. What the campaign licenses

A quality reading on the TLSF path at generation 10 and population 200, at the shipping selection scheme and status grading, over these 21 families. It does not license a FRETISH claim, a claim at another search size, or a claim about `nsga2-truncate`, whose pool is 65% to 84% duplicate objective vectors and whose front holds 57% to 70% of the pool — a front the constraint would act on very differently.

## 8. Threats to validity

The constraint removes the similarity-against-status trade from the front, and that trade is what `2026-07-24-ablation` and `2026-08-29-aurus-matched` credit NSGA-II's quality edge over `weighted` to. A loss here is therefore the expected direction of the risk, not a surprise.

Feasibility is read off the last objective in the vector. Both front ends register status last and register it only when its weight is positive, and this campaign weights it at 0.5, so the coupling holds for every row here. A run weighting status at zero would leave the ranking unconstrained rather than constrain it on the wrong axis.

The 21-of-21 front measurement is 7 families at 3 seeds, which establishes that the original is always on the front and not how much it costs. This campaign measures the cost.

## 9. Deliberately not covered

ε-dominance, which the 364x margin spread argues against as a single-parameter fix and which would want its own arm. Barring the original from rank 0 by name, which is narrower and cheaper and worth running if this arm reads null. The assumption-side status grading, which is a change to the objective rather than to the ranking.

## 10. Provenance

Branch `campaign/constrained-domination`. Hosts av2 (seeds 0-14) and av3 (seeds 15-29), `jobs = 8`, profile `constrained-dom`, sweep K. Queued through `scripts/campaign.py enqueue`, so the tick starts it and `campaign.py status` is the record of what it did.
