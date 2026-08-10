# Elitism default: `elitism_rate` 0.0 vs 0.1

Pre-registered 2026-08-07, before any row at the campaign commit was collected. Written to bind the decision rule to the result it was meant to decide.

## 1. Question

`Config::elitism_rate` ships at 0.1. Should the shipped default be 0.0 or 0.1?

This is a production-defaults question, not a search-behaviour question. `counter` is meant to be push-button: a user who writes no config at all gets these values, so the default has to be the one that is best across a corpus rather than the one that is best on the spec that happened to motivate it.

## 2. Why the question is open

The design argument says 0.0. Both NSGA-II schemes pool survivors (μ+λ) and are already elitist, so carrying the top fraction over verbatim is redundant on paper; worse, elites bypass the offspring filter chain entirely, including the vacuity and well-separation screens.

Against that stood exactly one measurement. On `examples/lily02` at seed 42, `elitism_rate = 0` returned a repair whose guarantee was `req W (F(G cancel | G true))`, which reduces to `true` and so deletes a guarantee outright. That anecdote (n=1) is what the comment in `include/config.hpp` and `example-config.toml` currently cites as the reason the default stays at 0.1.

Two things have changed since that anecdote, and both have to be accounted for.

**The anecdote's failure mode is now screened out by construction.** Commit `3869c53` (2026-08-07) added a tautology screen to the vacuity filter, and the final repair screen applies the vacuity predicate unconditionally — `tlsf_is_vacuous` in `src/tlsf/survivors.cpp:36`, `specification_is_vacuous` via `is_realizable_repair` in `src/repair/evolution.cpp:34`. `tlsf_has_valid_guarantee` decides exactly the `G true` case that the lily02 repair exhibited. The screen runs whether or not the per-generation filter is enabled and whether or not a candidate arrived as an elite, so the specific defect that argued for keeping 0.1 should no longer be reachable at the campaign commit. The campaign has to confirm that rather than assume it.

**`implies_ideal` measures gutting after all, contrary to the note that parked this A/B.** `compare` classifies a repair as `Stronger` when the *repair* implies the *ideal* (`src/compare.cpp:228`, called at `:354` and `:407` with the repair as `from`). A gutted repair admits more behaviour than the ideal, so it reads as `Weaker`, and `implies_ideal` counts only `equivalent + strictly stronger` (`scripts/run_experiments.py:1104`). On the TLSF path this is exact: `tlsf_spec_implies` lowers the whole spec to one LTL formula rather than decomposing it. So gutting is penalised by the primary quality metric, and the A/B does not need a new CSV column to see it.

## 3. Prior evidence and what it does not settle

Sweep R already ran as a nuisance factor inside the `replicate` campaign (2026-07-31, commit `231f822`). Recovered from av2/av3: 4800 FRETISH rows and 1200 TLSF rows, 6000 distinct cells. Paired by `(spec, seed)` within scheme:

| path | scheme | n pairs | `found_repair` 0 → 0.1 | `implies_ideal` 0 → 0.1 | wall ratio (0.1 / 0) |
|---|---|---|---|---|---|
| FRETISH | nsga2-truncate | 800 | 1.000 → 1.000 | 0.670 → 0.672 | 0.88 |
| FRETISH | nsga2-apportion | 800 | 0.998 → 1.000 | 0.776 → 0.776 | 0.69 |
| TLSF | nsga2-truncate | 300 | 0.800 → 0.800 | 0.200 → 0.200 | 0.87 |
| TLSF | nsga2-apportion | 300 | 1.000 → 1.000 | 0.167 → 0.160 | 0.88 |

Quality and yield are flat everywhere; the discordant-pair counts are near-symmetric (FRETISH/truncate: 57 vs 55). The one consistent effect is cost, and it favours 0.1 by 12–31%.

That prior does not close the question, for three reasons.

1. **Commit vintage.** Those rows predate the tautology screen (`3869c53`), the correct-offspring rescue fix (`01a4308`), the realizability-timeout resolution change (`8440904`), and the flip of `run_well_separation` and `allow_output_assumptions` to `true` by default (`86e463c`). Well-separation in particular interacts with elitism directly, because elites bypass it.
2. **Corpus width.** Five TLSF families and four FRETISH families. The fixes-backed corpus is now 20 TLSF families and 4 FRETISH ones. A default has to hold across the corpus, and TLSF/truncate showed *zero* discordant pairs across 300 — a corpus too narrow to move.
3. **Triviality was never audited.** The prior campaign recorded no check that written repairs are non-trivial. That is the whole substance of the original objection.

So the campaign re-runs the factor at the current commit, over the widest corpus available, with a triviality audit attached.

## 4. Design

Two profiles, one per path. `elitism_rate` is the only factor; everything else sits at the shipped default.

| | `elitism-fret` | `elitism-tlsf` |
|---|---|---|
| corpus | 4 FRETISH families | 20 TLSF ablation families |
| operating point | gen 40 / pop 1000 | gen 10 / pop 200 |
| scheme | `nsga2-truncate` | `nsga2-truncate` |
| arms | `elit0`, `elit0.1` (sweep R) | `elit0`, `elit0.1` (TLSF sweep R) |
| seeds | 150 | 40 |
| runs | 4 × 2 × 150 = 1200 | 20 × 2 × 40 = 1600 |
| jobs | 4 (`parallel` 8) | 1 |

**Scheme is fixed at `nsga2-truncate`, not crossed.** It is the shipped default, and the question is about the shipped configuration. Crossing `nsga2-apportion` would double the campaign for an arm no user gets by default; the prior data above already shows the elitism effect does not change sign between schemes.

**Seeds are split seed-major and disjoint across av2/av3**, so a host lost mid-campaign still leaves a balanced design over every spec and arm.

**One deviation from the shipped defaults, recorded rather than fixed.** `gen_configs.py`'s baseline pins the fitness weights at 0.33 / 0.33 / 0.1 / 0.33 rather than the `config.hpp` defaults of 0.2 / 0.5 / 0.1 / 0.5, along with `model_counting.default_bound = 20` and `runtime.black_timeout_ms = 1000`. These are campaign conventions shared with every prior sweep, and changing them here would make this campaign incomparable with the `replicate` rows in §3. Under `nsga2-truncate` the weights only select which components are active — all four are non-zero in both settings — so they do not bias selection, and the difference reaches only the reported `best_fitness` scalar, which is not among the outcomes in §5. `run_vacuity` and `run_well_separation` are deliberately left unpinned, so both take the binary's current default of `true`.

## 5. Outcomes

**Primary.** `found_repair` and `implies_ideal`, paired by `(spec, seed)`, pooled within path.

**Secondary.** `wall_time_s` (the cost tie-break), `n_repairs`, and the `best_relation` distribution — the last because a shift from `equivalent` toward `strictly weaker` at fixed `implies_ideal` is the signature of gutting.

**Triviality audit (the one that answers the original objection).** Post-hoc, over every `repair_*` file the campaign writes, per arm: the fraction whose guarantee side reduces to `true` under `ltlfilt`. Run offline against the retained run directories, so it needs no C++ change and cannot affect the campaign. Pre-registered expectation: zero in both arms, because the final screen now decides this case. A non-zero count in `elit0` reproduces the lily02 anecdote at the current commit and settles the question on its own; a non-zero count in *either* arm is a bug in the screen and is reported as such rather than as an elitism result.

## 6. Power

Paired binary outcomes, McNemar. The FRETISH prior gives a discordance rate of 14% on `implies_ideal`.

- FRETISH: 600 pairs pooled (4 specs × 150 seeds) resolves ~0.043 absolute at 80% power.
- TLSF: 800 pairs pooled (20 × 40) resolves ~0.031 absolute at 80% power, assuming discordance no higher than 10%.

Both sit below the 0.05 non-inferiority margin fixed in §7, so the design can distinguish "no difference" from "a difference the margin would reject".

## 7. Decision rule

Fixed before launch. The burden of proof is on *changing* a shipped default.

**Switch the default to 0.0** if and only if all four hold:

1. **Non-inferior on quality.** The upper bound of the 95% CI on (`implies_ideal` at 0.1 − at 0.0) is below **+0.05** absolute, on both paths.
2. **Non-inferior on yield.** The same bound on `found_repair`, below **+0.05** absolute, on both paths.
3. **Clean triviality audit.** Zero trivial repairs in the `elit0` arm.
4. **Not materially more expensive.** Mean `wall_time_s` at 0.0 is no more than **10%** above 0.1, on both paths.

**Otherwise keep 0.1.** If it is kept, the justification comment in `include/config.hpp` and `example-config.toml` must be rewritten to cite this campaign and whichever criterion actually failed, replacing the n=1 lily02 anecdote. Leaving a stale anecdote in place as the reason for a shipped default is itself a defect, so this half of the outcome is a deliverable either way.

Criterion 4 is the one the prior data predicts will fail, at a wall ratio of 0.87–0.88. That is a legitimate reason to keep 0.1 and a materially different one from the reason currently recorded.

**Not covered by this campaign:** intermediate rates. The two arms are the two candidate defaults; nothing here says 0.05 or 0.2 is worse, and no claim about them will be made from this data.

## 8. Provenance

Campaign directory `experiments/2026-08-07-elitism/`, tracked contents: this `PLAN.md`, `PROVENANCE.json`, and `scripts/` (verbatim copies of `gen_configs.py`, `run_experiments.py`, `merge_experiments.py`, and the launch driver, with blob shas recorded).

Both hosts run `main` at the campaign commit, rebuilt before launch; `run_experiments.py` refuses to launch against a binary whose commit differs from the working tree's HEAD, and that gate is left on. Every CSV row carries `commit` and `dirty`.

The archived configs record only what sweep R overrides (`elitism_rate`); every other value comes from the binary's default at run time. Reproducing this campaign therefore requires the campaign commit, which `PROVENANCE.json` names.
