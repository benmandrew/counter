# 2026-09-04-metric-direct

Pre-registered 2026-09-04, before any row of this campaign existed.

## 1. The question

Does `model_counting.metric` change what the search finds? The paper's evaluation section carries a paragraph arguing that trace counts span many orders of magnitude, that a direct ratio saturates at the extremes, and that the logarithm restores the resolution the ratio loses. Nothing on the current corpus or the current engine measures that. The metric was crossed in `2026-07-16-metric` (800 runs) and in the 2026-07-24 ablation (1528 direct against 1548 log), both over the retired 21-family corpus, and both predate every engine change since 2026-08-14.

This is counter against counter. AuRUS takes no part in it, and no row of AuRUS's archive is read by the analysis.

## 2. Design

One arm. `model_counting.metric = "direct"` over the 25 `H2H_TLSF_READY` families at seeds 0-29, 750 rows, at `termination = "individuals"`, `max_individuals = 1000`, population 100, `runtime.parallel = 1`, weights 0.1/0.2/0.7, weakening screen off, `selection_scheme = "nsga2-apportion"`, `fitness.status_grading = "mrs"`.

The `logarithmic` half is not run. It is the `nsga2-apportion`/`mrs` cell of `2026-08-29-aurus-matched`, 750 archived rows at this operating point, and the contrast is read paired on `(spec, seed)` against it. The generated config differs from that cell's archived `config.toml` in one line, `metric`, which was checked by diff before this campaign was declared.

## 3. Why `ec0abe0`

The branch is cut from `ec0abe0`, the commit the archived arm was built from, rather than from `main`. Two commits since then change scoring: `11305f9`, which skips the simplify pass on TLSF whole-spec queries, and `2defe77`, the `compare` fix that the aurus-matched archive was deliberately not re-scored with. Building this half at either would score the two halves by different `compare` versions and turn a metric contrast into a metric-plus-scorer contrast.

The cost of that choice is that this arm inherits the old `compare` and its timeouts. Section 6 registers what that does to the endpoint.

## 4. Primary endpoint

`implies_ideal`, paired on `(spec, seed)` over the cases scorable in both arms, read with an exact two-sided *McNemar* test at alpha 0.05. Discordant counts and the tied count are reported beside every p-value, because a corpus with families at the floor and the ceiling produces ties for reasons that have nothing to do with the metric.

Secondary, reported as measured and not tested against a rule: `found_repair` on the same pairing, `wall_time_s` as a paired ratio, `n_repairs`, and the four anytime curves at the cuts `score_curves.py` writes.

## 5. Decision rule

- **Outcome 1, direct higher at p < 0.05.** The evaluation section's logarithmic-scoring paragraph is wrong on this corpus and gets rewritten to say so. `model_counting.metric` is reopened as a default, in its own campaign rather than here.
- **Outcome 2, logarithmic higher at p < 0.05.** The paragraph stands and cites this campaign as its measurement.
- **Outcome 3, null.** The paragraph is rewritten to report a null, with the discordant counts and the achieved power from section 6. A null is not evidence that the metric is inert; on this corpus it is mostly evidence about the corpus.

No outcome changes a shipped default on its own. This campaign measures one arm against an archive, and a default move wants a cross.

## 6. Registered hazards

- **Power is thin and the corpus is why.** In the archived cell, 8 of 25 families sit at the ceiling on `implies_ideal` in all four aurus-matched arms and 10 sit at the floor. Seven carry any contrast at all: `arbiter-aurus`, `full-arbiter-aurus`, `humanoid-531`, `lily11`, `load-balancer-aurus`, `minepump`, `prioritized-arbiter-aurus`. An effect confined to the other 18 is not detectable here at 30 seeds.
- **The cap is inherited.** 70 of the archived cell's 750 runs were killed at 7200 s, 55.3% of that cell's 253.4 core-hours. A kill scores `implies_ideal = 0`, so a metric that is merely slower loses the endpoint without ever being worse at the search. The paired analysis therefore also reports the contrast over cases finishing in both arms, and the two reads are given together whether or not they agree.
- **`compare` timeouts are inherited.** The archived cell carries runs whose ideal metric is blank because `compare` exceeded its cap, and an undecided candidate-ideal pair counts as not implying. Every `implies_ideal` count on both sides is a lower bound.
- **`humanoid-742` will contribute nothing.** It was killed on all 30 runs of the archived cell. Expect the same here and read the family as censored rather than as a zero.

## 7. Cost

The archived logarithmic cell cost 253.4 core-hours over its 750 runs, at a mean of 1216 s. Paired over 382 cases of the July ablation, direct ran 1.027x log, so this arm is budgeted at about 260 core-hours. At the effective concurrency aurus-matched achieved, 31.1 across av2 and av3 at `jobs = 16`, that is roughly 8.4 h of wall clock. Six families are 89% of the bill: `humanoid-742` 60.0 core-hours, `humanoid-531` 57.8, `pcar-v2-888` 41.4, `humanoid-503` 23.4, `full-arbiter-aurus` 23.3, `prioritized-arbiter-aurus` 20.7.

The July ratio was measured at gen40/pop1000 on the retired corpus with a 600 s cap and a 95 s mean, against 1216 s here, so it carries as a direction rather than as a guarantee at this tail. If `direct` degrades the search on the heavy families and capped runs go from 70 to 150, the arm costs about 420 core-hours and 13 h. The ceiling, every run capping, is 1500 core-hours.

## 8. What this campaign does not do

It does not turn `filters.run_implication` off. That key is read only by `get_final_filter_functions` and applied once to the final realisable population, so an off arm would reproduce the same search and change only what was printed. The compression it performs is already measured by `solutions` against `maximal_solutions` in the aurus-matched curves.

It does not vary the selection scheme or the status grading. Both were crossed in `2026-08-29-aurus-matched` at this operating point.
