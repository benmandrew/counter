# PLAN — well-separation filter timing

Campaign plan for the well-separation filter's *schedule*: whether it runs every
generation or once at the end. The filter itself is unchanged in every arm — the
same accept/reject predicate, applied on a different cadence — so the comparison
is search quality against cost. Launched 2026-08-06 on av2 and av3 from branch
`feat/wellsep-timing`, binary `cbcaede`.

## 1. The question

Per-generation filtering pays an `ltlsynt` call per surviving candidate every
generation, and in exchange the population that breeds is one the filter already
accepts. Final-only filtering pays that cost once. It risks spending a whole run
breeding candidates that all die in the single closing pass.

The 2026-07-23 wellsep campaign settled *whether* to run the filter and said
nothing about *when*. No campaign has varied the cadence.

## 2. Design

Three arms, the three levels of TLSF sweep V
(`TLSF_SWEEP_V` in `scripts/gen_configs.py`), all at
`allow_output_assumptions = true`:

| Arm | Config | Meaning |
|---|---|---|
| `nofilter` control | `run_well_separation = false` | the filter never runs |
| `every-gen` | `well_separation_interval = 1` | the shipped behaviour |
| `final-only` treatment | `well_separation_interval = 1000` | one pass, on the last generation |

The treatment arm needs no code change. Both drivers force every per-generation
filter to run on the last generation whatever its interval:
`filters_for_generation` (`src/genetic/generation.cpp:162`) and
`select_active_filters` (`src/tlsf/pipeline.cpp:271`), with generation numbering
1-based at both call sites. An interval of 1000 sits above the 10 generations
this campaign runs, so the cadence never fires and the forced last-generation
pass is the only one. Expressing the arm as an interval rather than a mode keeps
it final-only if the operating point later moves.

Grid: 16 TLSF specs × 150 seeds × 3 arms = 7200 runs, at `gen10/pop200` under
`nsga2` selection. Seeds 0–74 on av3, 75–149 on av2 — disjoint and seed-major,
so a deadline kill leaves a balanced design. Specs: arbiter, gyro-var1, lift,
lily02, minepump, arbiter-aurus, arbiter-handshake, codesample-un1,
codesample-un2, detector, gyro-var2, humanoid-458, load-balancer, rg1, rg2,
round-robin-arbiter.

Five specs are excluded on cost — amba, full-arbiter, humanoid-531,
prioritized-arbiter and simple-arbiter each exceeded 90 s on a single seed under
the expensive arm. takeoff-tlsf is excluded for having no ideal fix under
`examples/<spec>/fixes`, which would leave `implies_ideal` undefined on part of
the grid.

## 3. Why `allow_output_assumptions` is fixed on, not crossed

The archived 2026-07-23 campaign (`experiments/2026-07-23-wellsep/`) measured
the filter inert with input-only assumptions and decisive with
output-referencing ones:

| arm | found-rate | mean `n_repairs` | mean wall |
|---|---|---|---|
| wsoff-oaoff | 79.7% | 3.97 | 13.3 s |
| wsoff-oaon | 99.8% | 5.03 | 17.6 s |
| wson-oaoff | 79.6% | 4.17 | 13.2 s |
| wson-oaon | 79.6% | 4.17 | 16.2 s |

Input-only assumptions are always well-separated, so with the output path shut
the filter has nothing to reject. Timing an inert filter measures nothing. The
factor is therefore held at the level where the filter has work to do, and no
level of sweep V is byte-identical to the A/gen10 baseline, so all three arms
execute rather than aliasing onto an existing run.

## 4. The pilot that motivated the hypothesis

This plan is not blind. A 12-run smoke test on four specs, three seeds each,
ran before launch; the figures are totals of `n_repairs` over the three seeds.

| spec | `nofilter` | `every-gen` | `final-only` |
|---|---|---|---|
| arbiter | 20 | 0 | 16 |
| gyro-var1 | 36 | 38 | 36 |
| lily02 | 38 | 24 | 36 |
| minepump | 28 | 34 | 28 |

The arbiter column is the motivating observation: filtering every generation
removed every repair, filtering once at the end kept most of them. The working
hypothesis is that per-generation filtering prunes non-well-separated candidates
that are useful stepping stones, so the search never reaches the well-separated
regions final-only reaches. gyro-var1 and minepump move in the other direction
or not at all, which is why the grid was widened from wellsep's five specs to
sixteen — a narrow grid would report whichever way its specs happened to lean.

The cadence itself is confirmed working. In a final-only run of arbiter the
filter report read `not-well-separated  25 in  17 out  32.0% avg drop`: one
generation's worth of candidates, filtered once, dropping 32%.

What is *not* confirmed is that final-only's emitted repairs are well-separated,
and the pilot must not be read as evidence that they are. Issue #73 records that
`stage_restore_elites` appends elites after every filter stage
(`include/genetic/pipeline.hpp:363-371`) and that neither final gate re-screens
them — `is_tlsf_repair` (`src/tlsf/pipeline.cpp:60-63`) tests unsatisfiable
assumptions and status, not well-separation. Elites therefore reach the output
unfiltered on both arms.

This is a live threat to the headline result rather than a footnote. Final-only
breeds nine generations with no well-separation pressure at all, so its elite
pool can carry not-well-separated specifications the whole way to the output,
and its apparent advantage on arbiter would then be an artefact of that leak
rather than the stepping-stone effect hypothesised above. Two further leaks
compound it: `stage_filter_fallback` re-admits the entire unfiltered offspring
set when the chain empties the population (#72), which fires most often when the
filter is working hardest, and a realizability timeout is read as
well-separated and cached for the rest of the run (#74) — material here because
this grid runs at `ltlsynt_timeout_ms = 500`. Issue #77 adds that the seed
population is never filtered at all.

Criterion 1 below is therefore the campaign's gate, not a formality: the
hypothesis stands or falls on whether final-only's repairs survive an
independent check.

## 5. Launch

```sh
python scripts/gen_configs.py --tlsf --sweeps V \
    --out-dir experiments/configs-wellsep-timing

python scripts/run_experiments.py --profile wellsep-timing --seeds $(seq -s' ' 75 149) # av2
python scripts/run_experiments.py --profile wellsep-timing --seeds $(seq -s' ' 0 74)   # av3
python scripts/merge_experiments.py av2 av3 --profile wellsep-timing
```

Sized from a measured 181 job-seconds per seed per arm over these 16 specs at
the campaign's own geometry (4 concurrent runs, `parallel = 8`), so 543
job-seconds per seed across the three arms. Two 32-core hosts at 4 jobs supply
172800 job-seconds in six hours, and the seed count leaves headroom for the
`compare` work the calibration excluded. The range is deliberately under-sized:
resume keys on `(sweep, level_name, selection, weakening, metric, repair_mode,
spec, seed)`, so extending it later costs only the new cells, whereas
over-sizing ends the window with the arms unbalanced and breaks pairing.

Per-spec timeout caps run from 60 s to 600 s for lift, roughly 5× the measured
single-seed cost under the expensive arm. The profile writes its own
`results-wellsep-timing.csv` and results directory; it must not share wellsep's,
since the two campaigns reuse the same specs and seeds under the same selection
scheme and `run_id` names collide across profiles.

## 6. Analysis and decision rule

Pre-registered, so the analysis cannot be steered by its own result. Pairing is
by `(spec, seed)` across arms; stratification is by spec throughout. CMH below
is the Cochran–Mantel–Haenszel test, and CI a 95% confidence interval.

1. **Soundness.** Every repair `final-only` emits must be well-separated. This
   is *not* structural: the forced last-generation pass screens bred offspring
   only, and issues #72, #73, #74 and #77 each describe a route by which a
   not-well-separated specification reaches the output anyway. The check is
   therefore measured, not assumed. Every emitted `repair_*.tlsf` on both
   filtered arms is re-checked independently, outside the run that produced it,
   and the leak rate is reported per arm per spec before any other statistic.

   The rate is the gate. If `final-only` leaks materially more than `every-gen`,
   the two arms are not comparable on quality or yield, because final-only is
   then buying its repairs by emitting ones the filter would have rejected;
   criteria 2 and 3 are void and the reported result is the leak itself. If both
   arms leak at a similar low rate, the comparison stands and the leak is
   reported as a caveat bounding it. A clean `final-only` is the only case in
   which the stepping-stone hypothesis survives.
2. **Quality non-inferiority.** `final-only` must not lose repair quality
   against `every-gen`. Primary metric is the per-run binary `implies_ideal`,
   tested by CMH stratified by spec. The *non-inferiority margin* is 2
   percentage points: the criterion passes if the CI lower bound on
   (final-only − every-gen) exceeds −0.02.
3. **Yield.** The `found_repair` rate for `final-only` must be at least that of
   `every-gen`, same CMH test, one-sided. The direction is reported per spec,
   because the pilot suggests the effect changes sign across specs.
4. **Cost.** Median paired wall-time ratio `final-only / every-gen`, Wilcoxon
   signed-rank on the paired times. `final-only` makes one `ltlsynt` pass
   instead of ten and is expected to be cheaper; the criterion passes if the
   median ratio is at or below 1.0 with p < 0.05.

The shipped default becomes final-only only if criteria 1, 2 and 4 all pass and
3 does not regress. A mixed result is reported per spec rather than pooled into
a single recommendation, since the pilot already shows the sign varies by spec.
`nofilter` is a control on both responses: it bounds what the filter costs and
what it removes, and neither treatment arm is judged against it.

## 7. What would make the result uninformative

Two failure modes void the comparison rather than answering it.

If the filter's drop rate is near zero across the grid, the three arms are not
distinguishable and the campaign says nothing — the same inertness the
2026-07-23 campaign found without output assumptions, arriving through a
different route. The final-generation drop rate from criterion 1 is the
diagnostic, and a grid-wide rate near zero is a null result about the design,
not about the schedule.

If a large share of runs hit their timeout cap, the wall-time comparison is
*censored* rather than measured. A cap that binds on one arm and not the other
is a one-sided filter on the response under test, because a censored run records
`found_repair = 0` and `implies_ideal = 0`. The 2026-07-31 replicate campaign
lost 379 rows exactly this way, and its pooled McNemar result read p ≈ 5e-37
censored against p = 0.68 on the uncensored pairs. Timeout counts are therefore
reported per arm per spec before any other statistic, and an arm-asymmetric
count retires criterion 4 instead of being analysed around.

The campaign asks one narrow question, and a per-spec answer is a real answer to
it — the filter's cadence may simply be the wrong knob to hold fixed across a
corpus this varied. What it cannot say is whether some intermediate interval
beats both ends, since only 1 and 1000 are on the grid.
