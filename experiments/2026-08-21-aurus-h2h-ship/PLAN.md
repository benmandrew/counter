# counter at the shipping configuration against AuRUS: the head-to-head re-read at full power

Pre-registered 2026-08-21, before any row of this campaign existed. Section 5 fixes the decision rule, section 6 registers a hazard in the endpoint that could otherwise be discovered afterwards, and both stay checkable against whatever rates arrive.

## 1. Question

AuRUS repairs unrealizable *Temporal Logic Synthesis Format* (TLSF) specifications by genetic search, over the same inputs counter reads. `experiments/2026-08-14-aurus-h2h` compared the two over AuRUS's own 25-family corpus and found AuRUS higher on repair quality: p = 0.0127 over families and p = 0.0195 over clusters, at mean rates of 0.281 for counter against 0.504 for AuRUS.

Three changes have become permanent since that arm ran. The *accumulator*, which carries realizable candidates forward across generations, became the default in `37b1f0e` on 2026-08-19. The repaired mutation and crossover grammar became unconditional in `1af9f45` and `6e5b710` on 2026-08-20. The weakening screen's default went to false in `e3218a1` on the same day. The 2026-08-14 counter arm had none of the three.

The campaign asks whether counter, at the configuration it ships on 2026-08-21 plus the accumulator, still loses to AuRUS on repair quality over that corpus.

## 2. Why the existing rows do not answer it

`experiments/2026-08-20-ops-weakening/REPORT.md` reads the two sweep-O campaigns against the archived AuRUS rows, which is what `2026-08-20-ops-grammar/PLAN.md` sections 4 and 10 committed to before those campaigns ran. That read is restricted to the 10 families the sweep-O corpus shares with this one, and those 10 fall in 7 of the 10 clusters.

Seven clusters put the exact two-sided p floor at 0.0156, and reaching it requires every cluster to point one way. Every arm read null under that restriction, including the archived arm itself at p = 0.1562. The restriction therefore dissolves the archived verdict before any arm change does. The restricted read is a power loss rather than a result.

The subset is fair on effect size. The mean gap between the two tools is 0.222 over the 10 included families and 0.224 over the 15 excluded ones, so the shared corpus is not the easy half or the hard half.

The sweep-O grid also introduced three confounds against the archived arm: `nsga2-truncate` against `nsga2-apportion`, the direct similarity metric against the logarithmic one, and `run_well_separation` on against off. None of the three is a change this campaign is asking about.

## 3. The arm and its configuration

One arm, at the shipping configuration plus the accumulator. `gen_configs.py` emits exactly this:

| key | value | key | value |
|---|---|---|---|
| `generations` | 10 | `default_bound` | 20 |
| `population_size` | 200 | `metric` | `logarithmic` |
| `selection_scheme` | `nsga2-apportion` | `run_weakening` | false |
| `crossover_rate` | 0.1 | `run_implication` | true |
| `mutation_rate` | 1.0 | `black_timeout_ms` | 1000 |
| `accumulate_repairs` | true | `ltlsynt_timeout_ms` | 10000 |
| the three fitness weights | 0.33 each | `ltl2tgba_timeout_ms` | 60000 |
| `p_trigger` | 0.5 | `max_scoring_failure_rate` | 0.15 |
| `p_response` | 0.5 | `repair_mode` | `monolithic` |
| `p_timing` | 0.15 | | |

The corpus is `H2H_TLSF_READY`, 25 families, at seeds 0 to 19 split av2 0-9 and av3 10-19. The wall cap is 7200 s, matched to AuRUS's `-GATO`, at `jobs = 8` and a `compare_timeout` of 1800 s.

**Two keys the config does not state.** `run_well_separation` and `allow_output_assumptions` are absent and take the binary's defaults, which are false since `b101ada` on 2026-08-10 and true respectively. This paragraph is the config-vintage record for this campaign in the sense `experiments/README.md` uses, since an archived config holds only what a sweep overrode and every other value comes from the binary at run time.

`--pin-vintage` is deliberately not passed. It writes `run_well_separation` from `gen_configs.DEFAULTS`, whose entry has read true since the binary default moved, so pinning would put the per-generation well-separation filter into the one arm meant to run counter as it ships. That is how the filter reached the 2026-08-14 arm, against that profile's own stated intent of tracking the shipping defaults.

## 4. The control, and what it cannot do

The control is the 499 archived `aurus-h2h` counter rows: the same 25 families, the same seeds 0 to 19, the same 7200 s cap, the same two hosts and the same seed split. No second arm runs here.

The two configurations differ in exactly four things — `accumulate_repairs`, `run_weakening`, `run_well_separation`, and the operator grammar, which is unconditional in the binary rather than a key. This is a bundle test. It attributes nothing to any one of the four. `2026-08-20-ops-grammar` and `2026-08-20-ops-weakening` already separate the grammar and the screen on a smaller corpus, and `2026-08-19-accumulator` separates the accumulator; this campaign asks only what the bundle is worth against AuRUS.

## 5. Endpoint and decision rule

The primary endpoint is the per-family repair-quality rate: counter's `implies_ideal` over its 20 seeds against AuRUS's `implies_genuine` over its 30 repeats, compared by a two-sided exact *Wilcoxon signed-rank test* over families at alpha 0.05, and by the same test over the 10 clusters of the head-to-head's section 7.10. AuRUS's rate is over the repairs that are both realizable under `realize` and well-separated under `check_well_separated`, following that plan's section 10.1, because counter's output gate rejects an ill-separated survivor unconditionally. The statistic is computed by `scripts/analyse_aurus_h2h.py` unchanged, over this campaign's merged CSV placed in this directory.

Three outcomes, and the campaign takes exactly one.

1. **counter higher** at p < 0.05, reported as a measured advantage on this corpus at this operating point.
2. **AuRUS higher** at p < 0.05, reported as measured, with no re-run and no post-hoc arm added to recover it.
3. **Neither at p < 0.05**, in which case the 2026-08-14 result does not reproduce at the shipping configuration, and the campaign says that and no more.

Where the per-family and clustered tests disagree, the clustered result governs, following the head-to-head's section 10.2.

## 6. A registered hazard in the endpoint

As counter improves it ties AuRUS at the ceiling on more families, and the Wilcoxon drops a tied family from the test. A better arm can therefore present a smaller n and a weaker p than a worse one. The archived arm had 19 non-tied families of 25 and 6 tied.

Registered now: the sign counts, the tie count and the two mean rates are reported alongside the p value in every one of the three outcomes, and outcome 3 is read against them rather than on its own.

## 7. Secondary measures, none gating

The paired counter-against-counter comparison with the archived arm on the same `(spec, seed)`, by exact *McNemar test* on `implies_ideal`, which says what the four changes bought independently of AuRUS. Yield. `n_repairs`, where the archived arm ran a median of 4 per scoring run and the accumulator with the screen off ran 7 over the sweep-O corpus. The median paired wall ratio. The timeout rate at the 7200 s cap, which was 0 of 499 on these families in the archived arm. The `compare` timeout count, which the raised 1800 s budget exists to keep at zero.

## 8. Threats to validity

**AuRUS is not re-run.** It is not seedable, its arm is 30 repeats rather than seeds, and its 2026-08-14 rows ran at `-k=20`, the published model-counter bound, unlike the July campaign's `-k=10` that `experiments/2026-07-24-ablation/REPORT.md` records.

**The control rows are six days older.** The hosts have run other campaigns since. The paired secondary of section 7 cannot control for that, and the primary endpoint does not need to, comparing two rates rather than two runs.

**`run_weakening = false` means a written repair may forbid behaviour the original allowed.** That is the shipping default, and `2026-08-20-ops-weakening` measured its cost as 38 gains and no losses over 720 matched runs.

**`run_well_separation = false` gives the search no gradient off ill-separation.** The output gate enforces the property unconditionally regardless, so output correctness is unaffected.

**The ideals are a curated set.** Both tools are scored by the same `compare` against `examples/<spec>/fixes` with no `-ref` flags, so the comparison is one statistic over two populations, and an incomparable family biases `implies_ideal` for both arms equally.

## 9. Cost

The archived counter arm over the same corpus, seeds and cap ran 4 h 12 m on av2 and 4 h 57 m on av3 at `jobs = 8`. The accumulator adds a gate sweep per generation on the TLSF path, and the screen coming off raises what reaches `compare`. Budget six to eight hours per host, and expect the campaign to close inside a day.

## 10. Provenance

Branch `campaign/aurus-h2h-ship`, declared in `campaign.toml` beside this file, launched through `scripts/campaign.py enqueue` so the seed split is never chosen at the prompt. The profile is `aurus-h2h-ship` in `scripts/run_experiments.py`, registered in `merge_experiments.PROFILE_CSVS`.

On close, vendor `gen_configs.py`, `run_experiments.py`, `merge_experiments.py` and `analyse_aurus_h2h.py` into `scripts/` beside this plan with their blob shas, and record whether the branch merged or was split.

A campaign whose control is another campaign's archive rests on the seed split, the cap and the corpus staying where they were put, which is why section 3 writes down two keys nobody set. Registering the tie hazard before the rates exist is the other half of that: an endpoint that can weaken as the arm improves is worth knowing about while the answer is still unknown.
