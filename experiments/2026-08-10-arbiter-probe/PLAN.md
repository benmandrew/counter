# Arbiter diversity probe: does `nsga2-apportion` unlock the family or one spec?

Pre-registered 2026-08-10, before any row at the campaign commit was collected. Written to bind the decision rule to the result it was meant to decide.

**Superseded execution.** This design ran once at `536a3ae` and finished 800 runs before three engine commits landed on `main`: `d7733fc` (written repairs gated on a shared correctness table), `fb4c3ed` (weak-until rewritten before `black` sees it, `black` having answered validity wrong on that operator), and `b101ada` (well-separation folded into the status score). The first changes what counts as a repair, which is this campaign's primary outcome; the third rescales an objective selection ranks on, and `experiments/README.md` records that quality figures do not compare across it. That data was discarded rather than archived, on the principle that a superseded measurement kept beside a current one gets cited. Nothing below was changed to fit it, and none of its numbers appear in this file.

## 1. Question

On `examples/arbiter`, `nsga2-apportion` repaired 120 runs of 120 where `nsga2-truncate` repaired 0 of 120. Does that hold across arbitration specifications, or is it a property of that one file?

This is a *generality probe*, not a defaults campaign. Nothing here can move `Config::selection_scheme`, and nothing here is designed to try.

## 2. Why the question is open

The result comes from the TLSF half of `2026-07-31-replicate`, where selection scheme was the factor under test. It is the only place either NSGA-II scheme has separated on `found_repair` at all — every other family in that campaign either found repairs under both schemes or under neither. The measured quality on the unlocked cell was `implies_ideal` 0.375, so the repairs were not merely present but sometimes right.

It is also the lever two earlier campaigns went looking for and missed. `2026-07-23-arbiter-hp` and `2026-07-23-arbiter-padd` both finished at zero repairs on the arbiter family, sweeping `p_add_assumption` across a 16-fold range (0.05 to 0.8) for a flat zero throughout. Those campaigns concluded that the mutation rate for adding a fairness assumption was not the bottleneck. The replicate rows suggest what is: *survivor diversity*, since apportion's only behavioural difference is that it deduplicates the survivor pool before ranking and apportions slots by `1 / (1 + rank)` rather than truncating.

Against that stands the sample. One family, one campaign, and a counter-signal in the same data: `lily02` fell from `implies_ideal` 1.00 under truncate to 0.44 under apportion. A scheme that unlocks arbitration while degrading everything else is a different recommendation from one that simply does better.

## 3. Prior evidence and what it does not settle

`2026-07-31-replicate` ran five TLSF families — `arbiter`, `gyro-var1`, `lift`, `lily02`, `minepump` — at gen 10 / pop 200, 60 seeds, both schemes crossed with both elitism levels. Three of the five were negative controls chosen because they sat at `implies_ideal` 0.000, so the campaign's TLSF half carried one family with yield headroom and one with quality headroom.

Three gaps keep that from answering §1.

1. **Corpus.** The corpus held six arbitration families at the time and the campaign sampled one. It now holds nine, six of which — `arbiter-aurus`, `arbiter-handshake`, `full-arbiter`, `prioritized-arbiter`, `round-robin-arbiter`, `simple-arbiter` — were imported or promoted after that campaign generated its configs.
2. **Engine vintage.** Those rows predate the tautology screen (`3869c53`), the fold of the false-condition filter into vacuity, and the flip of `run_well_separation` and `allow_output_assumptions` to `true` (`86e463c`). A scheme comparison run today is a different comparison, which is why both arms here are collected fresh rather than one arm being read off the archive.
3. **Mechanism.** Nothing in the replicate data shows *why* `arbiter` moved. This campaign does not answer that either; it establishes whether there is a family-level effect worth explaining before anyone spends a campaign explaining it.

## 4. Design

One profile, `arbiter-probe`. Selection scheme is the only factor; everything else sits at the campaign baseline.

| | `arbiter-probe` |
|---|---|
| corpus | 9 arbitration families + `lily02` |
| operating point | gen 10 / pop 200 (TLSF default) |
| arms | `nsga2-truncate`, `nsga2-apportion` |
| elitism | `elit0.1`, pinned |
| seeds | 40 (av2 0–19, av3 20–39) |
| runs | 10 × 2 × 40 = 800 |
| jobs | 1 |
| per-run cap | 900 s flat |

**The corpus is every fixes-backed family whose unrealizability is an arbitration conflict**: `arbiter`, `arbiter-aurus`, `arbiter-handshake`, `full-arbiter`, `prioritized-arbiter`, `round-robin-arbiter`, `simple-arbiter`, plus `amba` (a bus arbiter) and `load-balancer` (arbitration over servers). `arbiter` and `arbiter-aurus` are different problems despite the shared name — counter's own is a hand-written GR(1) mutex, the AuRUS import a request-response arbiter — and both are in scope for exactly that reason.

**`lily02` is the tenth family and is not an arbiter.** It is the counter-signal control. A probe restricted to families the scheme is expected to help would report the upside alone, and the one measured downside of apportion is on this spec.

**No compute-matched arm.** `2026-07-31-replicate` failed its decision rule on the compute-matched control and on a FRETISH wall-time ratio of 3.04 against a 2.0 bound. Those findings stand and this campaign does not reopen them; adding an arm C would imply it could. Cost is measured and reported here, not gated on.

**The cost ratio on this corpus is not replicate's 1.51.** A single paired run on `round-robin-arbiter` at seed 0, same host and binary, measured 18.47 s under truncate against 67.75 s under apportion — a ratio of 3.67. Replicate's 1.51 was pooled over five families, only one of which was an arbiter, so it does not transfer to a corpus that is nine-tenths arbitration. This is `n = 1` and is stated as a budgeting input, not a result; §8 calibrates before launch rather than trusting either figure.

**`elitism_rate` is pinned to 0.1 in the emitted config rather than inherited.** `2026-08-07-elitism` may yet move that default, and `gen_configs.py` writes a key only where a sweep overrides it — so an inherited value would leave the archived config silently restating whatever the binary later defaults to. Sweep R restricted to its `elit0.1` level writes the number down.

**Well-separation is a scored objective here, not a filter stage.** `b101ada` folded the property into the status score — `k_status_realizable` now requires a candidate to be well-separated as well as realizable, and one realizable only by defeating its own assumptions scores the middle tier — and restored `run_well_separation` to defaulting `false`. This campaign inherits that default, so no `not-well-separated` stage runs and the property reaches selection through the status objective instead. That is the shipped configuration, which is what a probe about a selection scheme should sample. It also matters more here than it would elsewhere: status is one of the objectives both NSGA-II schemes rank on, so the change alters the very gradient the two schemes sort by.

**One deviation from the shipped defaults, recorded rather than fixed.** `gen_configs.py`'s baseline pins `model_counting.metric = "direct"`, where `config.hpp` defaults to `Logarithmic`, along with fitness weights of 0.33 / 0.33 / 0.1 / 0.33 and `runtime.black_timeout_ms = 1000`. These are the campaign conventions every prior sweep shares, and `2026-07-31-replicate` ran under them too. Matching the campaign this one extends matters more than matching the shipped default, because the finding under test was measured there.

**Caps are sized never to bite.** Replicate censored its treatment arm alone, at caps cut to the control arm's costs, and manufactured a decisive result from the clock; apportion is the treatment arm here as well. The 900 s per-run cap is `elitism-tlsf`'s at the same operating point on the same hosts, and `compare_timeout` is raised to 1800 s because apportion returns more repairs and `compare`'s cost scales with them.

## 5. Outcomes

**Primary.** `found_repair`, paired by `(spec, seed)` within family. This is the response the replicate finding is stated in.

**Secondary.** `implies_ideal` on families where both arms find repairs, which is where a `lily02`-style quality regression would show; `n_repairs`; and paired `wall_time_s`.

**Reported, not decisive.** The `best_relation` distribution per arm, for the record.

## 6. Power

Paired binary outcome, McNemar, read as an exact sign test on the discordant pairs.

The effect under test is large by construction: on `arbiter` every one of the 120 replicate pairs was discordant and every one favoured apportion. Eight all-one-way discordant pairs already decide such a family at p = 0.008, so 40 seeds leave room for a *partial* unlock — a 30/10 split resolves at p ≈ 0.003.

What 40 pairs cannot resolve is a small shift: a 24/16 split reads p = 0.27. No claim at that scale will be made from this data, and the campaign is not sized to support one.

The same reasoning holds at the reduced seed count §8 allows on cost grounds: with 30 pairs a 23/7 split resolves at p ≈ 0.006, so the breadth criterion in §7 survives the cut.

## 7. Decision rule

Fixed before launch.

**The unlock is family-level** if both hold:

1. **Breadth.** At least 3 of the 9 arbitration families show `found_repair` higher under apportion at McNemar p < 0.05 after Holm correction across the nine.
2. **Direction.** No arbitration family shows the reverse at the same corrected threshold.

**The unlock is spec-specific** if `arbiter` reproduces (p < 0.05, apportion higher) and criterion 1 fails. **The finding does not reproduce at all** if `arbiter` itself fails to separate, which would make the replicate cell a property of that campaign's engine vintage rather than of the scheme.

Each outcome has a deliverable, which is the point of writing the rule down.

- *Family-level*: `docs/configuration.rst` gains guidance on selecting `nsga2-apportion` for arbitration specifications, stating the measured yield difference and the `lily02` quality cost, and the arbiter-hp / arbiter-padd null results are annotated with what the lever turned out to be.
- *Spec-specific*: the replicate `arbiter` cell is recorded in `experiments/README.md` as a single-family result that did not generalise, so it stops being cited as evidence about the scheme.
- *No reproduction*: the same, plus a note naming the engine changes between the two campaigns as the candidate explanation, which is a bug report rather than a scheme result.

**Quality is a reported caveat, not a gate.** If apportion unlocks families while `implies_ideal` falls on `lily02`, the guidance says both. Suppressing one to keep a clean recommendation is how the `n=1` anecdote in `include/config.hpp` came to stand for six weeks.

**Not covered by this campaign:** whether the mechanism is diversity, whether `1 / (1 + rank)` is the right weighting, anything about the FRETISH path, and anything about the shipped default.

## 8. Launch

Both hosts must be idle before launch — `2026-08-07-elitism` completed 2026-08-08 and `wall_time_s` is a response here, so nothing may co-schedule.

```sh
python scripts/gen_configs.py --tlsf \
    --schemes nsga2-truncate nsga2-apportion --sweeps R --levels elit0.1 \
    --out-dir experiments/configs-arbiter-probe

python scripts/run_experiments.py --profile arbiter-probe --seeds $(seq -s' ' 0 19)   # av2
python scripts/run_experiments.py --profile arbiter-probe --seeds $(seq -s' ' 20 39)  # av3
python scripts/merge_experiments.py av2 av3 --profile arbiter-probe
```

**Calibrate before launching.** The two available cost figures disagree by more than a factor of two — replicate's pooled TLSF ratio of 1.51 against the 3.67 measured on `round-robin-arbiter` in §4 — and the campaign is 800 runs at a 900 s cap, so the difference between them is the difference between one overnight run and three. Time a 20-run slice (both arms, five families, two seeds) on one host and project from that. Time it with jobs well above the worker count: a prior campaign timed 4 jobs on 4 workers and underestimated its cost by about 27%.

The order-of-magnitude expectation is 5–12 h per host. `elitism-tlsf` ran 800 runs per host over 20 families in 496 min at this operating point and `jobs = 1`, but both of its arms were truncate; this campaign is half apportion, over a corpus with no `humanoid` family in it. If calibration lands above 12 h per host, cut seeds from 40 to 30 before launching rather than after — §7's breadth criterion survives 30 seeds, and a mid-campaign cut does not leave a balanced design.

Fresh `results-arbiter-probe.csv` and results directory: resume skips by CSV key and never cleans output directories, so a stale row survives an engine change. Launch detached and guard on `ps comm` rather than `pgrep -f`. Run the `ltl2tgba` orphan janitor for the duration — it is a TLSF campaign, which is when the multi-GB orphans accumulate.

**The design survives an early stop, which matters more here than in a sweep.** Runs execute seed-major, then by spec, then by factor cell, so the two schemes of a given spec and seed run back to back. Checked over every one of the 400 prefixes of the av2 half: the two arms never differ by more than **1 run**, and once all ten families have been reached the spread across families never exceeds **2 runs** — one seed, both arms. A kill at any wall-clock deadline therefore yields the same design at fewer seeds, which §7 can still read, rather than a complete first arm and an empty second. Seed ranges across the two hosts are disjoint, so the same holds when both are stopped together.

## 9. Provenance

Campaign directory `experiments/2026-08-10-arbiter-probe/`, tracked contents: this `PLAN.md`, `PROVENANCE.json` written at launch, and `scripts/` — verbatim copies of `gen_configs.py`, `run_experiments.py` and `merge_experiments.py`, with blob shas recorded.

Both hosts run the campaign commit, rebuilt before launch; `run_experiments.py` refuses to launch against a binary whose commit differs from the working tree's HEAD, and that gate stays on. Every CSV row carries `commit` and `dirty`.

The archived configs record only what sweep R overrides, plus the `--tlsf` runtime settings; every other value comes from the binary's default at run time. Reproducing this campaign therefore requires the commit `PROVENANCE.json` names.
