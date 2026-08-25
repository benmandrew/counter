# The cross-generation repair accumulator: off against on, over the aurus-h2h corpus

Pre-registered 2026-08-19, before any row at the campaign commit was collected. The decision rule in §4 is fixed here so it stays checkable against the result it was meant to bind.

## 1. Question

counter emits the maximal antichain of its *final population* alone. A candidate that passed the output gate in generation 3 and lost its slot in generation 4 is a repair the search found and then discarded. `[genetic] accumulate_repairs` (new, default false) keeps every gate-passing candidate of every generation, deduplicated and uncapped, and unions them with the final population's collection before the final filters run.

The question is how much that union is worth on repair quality, and what it costs in wall time.

## 2. Design

Two arms over one corpus, paired by `(spec, seed)`. The arms differ in one config key and share a seed, so a difference between them is that key's.

| | `accoff` | `accon` |
|---|---|---|
| `accumulate_repairs` | `false` | `true` |
| corpus | 25 families (`H2H_TLSF_READY`) | same |
| operating point | gen 10 / pop 200 | same |
| selection | `nsga2-apportion` | same |
| metric | `log` | same |
| sweep level | N | N |
| seeds | 4 (av2 0–1, av3 2–3) | same |
| runs | 25 × 4 = 100 | 100 |
| jobs | 8 | 8 |
| cap | 240 s per spec | 240 s per spec |

Both arms state the key explicitly rather than taking it from the binary's default, through `--pin-vintage` on the `gen_configs.py` line in `campaign.toml`. The archive therefore records what this run was, whichever way the default moves afterwards.

The operating point, corpus and metric are `2026-08-14-aurus-h2h`'s, so these rows sit beside that campaign's. §5.2 states why they do not compare to them.

Both levels of a seed run on the same host, because the pairing is within a seed. The split in `campaign.toml` is over seeds and never over levels, which `run_experiments.py` crosses itself.

## 3. What the mechanism guarantees

Accumulation can only add to the emitted set, and repair quality is judged existentially: a run scores when at least one emitted repair implies an ideal. So `implies_ideal` and yield cannot fall. The bound is one-sided.

It bounds the direction and says nothing about the size. The discarded candidates may be dominated by what survived into the final population, in which case the final implication filter reduces the union back to the same antichain and nothing changes. A null result therefore says something specific, that the search does not lose repairs it later fails to rediscover, and is worth reporting as it stands.

## 4. Decision rule

Fixed before launch.

**Primary.** Per-run `implies_ideal`, paired by `(spec, seed)`, compared by a *McNemar's test* over the discordant pairs at alpha 0.05. The unit is the run rather than the family, because 4 seeds makes a family rate take only five values and ties would dominate the test.

**Cost bound.** The accumulator costs one gate sweep per generation on the TLSF path. The pre-registered bound is a median paired wall-time ratio at or below **1.25**. Above that, the key stays opt-in whatever the primary says, and a cheaper design becomes the follow-up.

Three outcomes, all reportable:

1. **`accon` higher, within the cost bound.** The key becomes a candidate default, subject to a FRETISH replication, this campaign measuring the TLSF path alone.
2. **`accon` higher, over the cost bound.** The key stays opt-in.
3. **No separation.** Reported as the null of §3, and not grounds for re-running with more seeds.

**Secondary measures**, reported and not gating: the paired difference in `n_repairs` per run by a *Wilcoxon signed-rank test*; the distribution of `n_accumulated_repairs`, which says how often the accumulator contributed anything at all and is read from each run's `run.json` rather than from the results CSV, `CSV_FIELDS` deliberately not being touched before a launch; and the paired wall-time ratio itself.

## 5. Threats to validity

Recorded before the fact, so none is discovered afterwards in the shape of whichever result arrives.

**5.1 The 240 s cap is a budget decision.** aurus-h2h ran this corpus at 7200 s, and its completed rows put the median run at 22 s against a p75 of 161 s and a p90 of 684 s, so the cap falls between the p75 and the p90 of the distribution it is applied to. The cap censors slow families identically in both arms, so the pairing holds. It cannot answer whether the accumulator's advantage grows with the budget, which needs a longer run and is out of scope here.

**5.2 The archived aurus-h2h rows are context.** The engine has moved since that campaign closed: `p_remove_guarantee`, the MRS status grading and the crossover graft all landed after it. Those rows therefore confound the accumulator with three other changes. The `accoff` arm is this campaign's only control.

**5.3 TLSF only.** `repair_mode = "muc"` is inert by design, since it evolves a core sub-specification and a gate-passing candidate of one is realizable against the core alone. The FRETISH path does not run here, though it is where the accumulator is free: its generation loop already asks the gate.

**5.4 Four seeds is thin per family.** The primary draws its power from 200 paired runs rather than from within-family precision. No per-family claim should be read off this campaign.

**5.5 `implies_ideal` is scored against counter's curated ideal set**, which is the sharpest threat aurus-h2h records. It applies identically to both arms, so it bears on the absolute rates and not on the paired difference.

## 6. Launch

Through `campaign.py`, off `experiments/2026-08-19-accumulator/campaign.toml`, which holds the only copy of the seed split.

```sh
python scripts/campaign.py stage 2026-08-19-accumulator
python scripts/campaign.py enqueue 2026-08-19-accumulator
python scripts/campaign.py status
python scripts/campaign.py collect --profile accumulator
```

Fresh results CSV and results directory before launch, since resume skips by CSV key and never cleans output directories, so a stale row survives an engine change. Run the `ltl2tgba` orphan janitor for the duration, this being a TLSF campaign.

## 7. Provenance

Campaign directory `experiments/2026-08-19-accumulator/`, tracked contents: this `PLAN.md`, `campaign.toml`, `PROVENANCE.json` written at launch, and `scripts/` — verbatim copies of `gen_configs.py`, `run_experiments.py` and `merge_experiments.py`, with blob shas recorded.

Both hosts run the campaign commit on branch `campaign/accumulator`, rebuilt before launch. `campaign.py stage` verifies each host's binary reports the declared commit with `dirty=0`, and every CSV row carries `commit` and `dirty`. The configs are generated on the host at stage time, the branch carrying the declaration and not the untracked config tree.

A one-sided mechanism makes it tempting to read any gain as vindication and any null as noise. Fixing the cost bound and the null's meaning before the rows exist is what keeps that reading from being negotiated after they arrive.
