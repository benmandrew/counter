# Experiments

A dated log of `counter` parameter experiments, newest first. Each entry records
**what changed**, **why**, and **what it found**. Raw data lives in the matching
gitignored `experiments-DD-MM-YYYY/` directory; analysis is `scripts/analyse.ipynb`.

The outcome metric throughout is `implies_ideal` — the fraction of runs producing
at least one repair *equivalent to* or *stronger than* an ideal.

---

## 2026-07-15 — Weakening filter as a crossed factor

**What changed.** `run_weakening` moved from a one-level sweep (the factorial's
sweep J) to a *crossed factor*: sweeps C, D, E, F and I ran at generations=40 /
population_size=1000 under NSGA-II, every level executed once with the weakening
filter on and once off. **Why:** the 2026-07-14 factorial's largest C–J effect
was the weakening filter, but it was read only at gen10/pop200 — the worst point
on both the *generations* and *population* curves — with no evidence it survived
at scale or how it interacted with the other parameters.

**Run.** Launched seed-major across av2 (seeds 0–44) and av3 (45–89) against an
18-hour budget, killed at the deadline (2026-07-16 12:00). Yield: **78 complete
seeds** (0–39, 45–82), 19,344 rows, 0 timeouts. Seed-major ordering makes the
survivors a *balanced* design — every cell sampled at the same 78 seeds — rather
than a ragged one.

### Result: disabling the filter raises `implies_ideal`

| Specification | off | on | Δ | Fisher p |
|---|---|---|---|---|
| **fsm** | 0.565 | 0.356 | **+0.208** | 7e-48 |
| **fsm-combined** | 0.096 | 0.063 | +0.033 | 3e-5 |
| fsm-timing | 0.958 | 0.955 | +0.003 | 0.62 (ns) |
| takeoff | 0.951 | 0.947 | +0.004 | 0.65 (ns) |

Pooled, 0.642 off against 0.580 on. The effect concentrates where there is
headroom; `fsm-timing` and `takeoff` sit near ceiling and do not move.

The mechanism is direct. `implies_ideal` counts repairs equivalent to or stronger
than the ideal, and the filter makes guarantees weaker — pushing them into the
*strictly weaker* class, the one class that fails the test. On `fsm`,
`best_relation` is `strictly weaker` for 62.9% of weakening-on runs against 5.1%
off, and `strictly stronger` for 38.3% off against 2.7% on. The filter does
exactly what it is named, and that is the whole effect.

### It replicates across the scale and engine change

The 2026-07-14 factorial, at gen10/pop200 and a *different* model-counting engine
(before `4ba4c91`), showed the same sign on `fsm`: 0.340 off against 0.030 on
(p≈7e-9). A 4×/5× change in generations and population, plus an engine change,
leave the direction identical. This is a structural property of the filter, not
an artefact of the operating point.

### The cost is speed, not success

| Specification | `found_repair` off/on | median `wall_time_s` off/on |
|---|---|---|
| fsm | 1.00 / 1.00 | 27.5 / 17.8 |
| fsm-timing | 1.00 / 1.00 | 36.5 / 25.1 |
| fsm-combined | 1.00 / 1.00 | 63.9 / 50.0 |
| takeoff | 1.00 / 1.00 | 11.9 / 11.8 |

Weakening off never fails to find a repair (100% `found_repair` in both arms) and
returns more repairs per run; its only cost is 30–55% more wall-clock. The filter
is purely a speed optimisation, trading ~20 percentage points of `fsm`
`implies_ideal` for ~30% faster runs. The caveat: all four specifications reach
100% `found_repair` without it, so weakening's intended job — rescuing
realizability when strong guarantees over-constrain — is never exercised here. On
this benchmark it is all cost.

### The surprise: a sign-flipping interaction with `status-only`

The `status-only` fitness preset, which weights only the status objective, is the
largest interaction in the data, and it flips sign. On `fsm` it scores 0.872 with
weakening off (best of any configuration) and 0.013 with it on (worst), p≈3e-9,
against a default of 0.615/0.372. With only the status objective active, nothing
penalises weakening the guarantee, so the filter weakens it to nothing; with the
filter off, the same narrow objective is the most effective preset tried.
`status-only` is viable *only* with weakening off.

### The SPOT bug fired more than expected, but does not bias the contrast

The pre-launch spot check found ~3 dropped individuals in a small sample; the full
campaign dropped **9,805**, up to 70 in a single run, touching 9.8% of rows. It is
spec-specific — `takeoff` never triggers it, the fsm family does — and stays
contained: 0 timeouts, 100% `found_repair`, no run aborted by the circuit breaker.
It also correlates weakly with the arm (11.75% of off rows affected against 7.95%
on), exactly the bias the design flagged. But the direction is reassuring: dropped
rows carry *lower* `implies_ideal` in both arms, and the off arm has more of them,
so the imbalance works against the measured off-over-on gap. The weakening
advantage is if anything understated.

### `fsm` and `fsm-combined` came off the floor

Baseline (weakening on) `implies_ideal` rose from 0.030 to 0.372 for `fsm` and
from 0 to 0.064 for `fsm-combined` against the 2026-07-14 factorial. The jump
confounds three changes — the larger operating point, the ideal-realizability fix,
and `4ba4c91`'s bound raise — so it cannot be attributed to any one, but the
magnitude is large.

### What this campaign cannot answer

- **Interactions are underpowered.** A per-level interaction carries a ±11-point
  95% interval at 78 seeds — wider than the effect it would explain. Sweep-level
  *trends* (pooling five levels) are readable; a single level that appears to flip
  is noise until a targeted follow-up says otherwise.
- **NSGA-II only.** Every conclusion is conditional on it; the factorial already
  settled the scheme comparison, and weighted is not the production scheme.
- **No comparison with the 2026-07-14 factorial.** `4ba4c91` changed the engine,
  so any cross-run difference confounds operating point with engine version
  ([[stale-results-after-engine-change]]). The scale-jump figures above are read
  in that light.
- **G (bound) and H (crossover) dropped** — ~28% of budget on parameters that
  showed a combined 2.6-point spread at pop200. The risk is that crossover may
  wake at pop1000, and H would have given the weakening × crossover interaction
  (both govern population diversity). Revisit this first.

### Method notes worth keeping

- **Cost was calibrated under a saturated queue** (32 jobs / 4 workers). A 4-job
  batch underestimated it by 27%, because short specs finish early and hand their
  cores to long ones — a bias absent at gen10/pop200, where the specs differ by
  ~2 s ([[calibration-saturation-bias]]).
- **Seed-major ordering** turns the deadline kill into a balanced design; the
  natural config-major order would leave the last levels at zero seeds.
- **Stale-binary hazard.** `build-release/counter` must be checked against
  fix-commit mtimes, not its checkout's HEAD; a binary three commits behind faked
  SIGSEGVs and hung `signal_tracer` processes ([[stale-binary-trap]]).
- **Isolation.** The profile writes its own `results_dir`, `configs_dir` and CSV,
  and `run_id` includes the weakening state, so the two arms never read each
  other's `repair_*.json`.
- **Timeouts are false zeros** (`implies_ideal=0`, `timed_out=1`); filter on
  `timed_out` before analysing. None fired this run.

### Scripts and launch

`gen_configs.py` gained `--generations`, `--population-size`, `--schemes`,
`--sweeps`, `--weakening` and `--out-dir` (defaults reproduce prior output
byte-identically). `run_experiments.py` gained the `cj-large` profile, a
`weakening` CSV column modelled on `selection`, per-profile directories and
baseline aliasing. `merge_experiments.py` gained the profile and — critically —
`weakening` in its merge key, without which the two arms collapse onto one key.

```sh
python scripts/gen_configs.py --generations 40 --population-size 1000 \
    --schemes nsga2 --weakening both --sweeps C D E F I \
    --out-dir experiments/configs-cj-large
# av2                                          # av3
… --profile cj-large --jobs 4 --seeds $(seq 0 44)   … --seeds $(seq 45 89)
python scripts/merge_experiments.py --profile cj-large av2 av3
```

**Verdict: disable the weakening filter for quality-focused repair.** It degrades
`implies_ideal` by making guarantees strictly weaker than the ideal, costs nothing
in repair success, and buys only runtime — on a benchmark that never puts it in
the situation it was designed for.

---

## 2026-07-14 — Factorial sweep A–J

**What changed.** A one-factor-at-a-time factorial: sweeps A (*generations*) and
B (*population size*), plus C–J holding everything else at gen10/pop200, each
level run under both NSGA-II and weighted selection, 100 seeds. Data:
`experiments-14-07-2026/results-factorial.csv` (25,196 NSGA-II rows).

**What it found.**

- **A and B climb monotonically** and are still rising at their top level
  (A: 0.480 → 0.576 over gen5–gen80; B: 0.348 → 0.527 over pop50–pop1500). So
  every C–J reading sits near the bottom of both curves.
- **C–J split three ways.** *Real signal* — J (weakening) 0.575 off vs 0.500 on,
  I (*mutation rate*) 0.280 → 0.500. *Signal only at degenerate extremes* — D at
  ptrig1.0 and F at ptim0.0 collapse to 0.007, each locking out a mutation
  dimension the repair needs. *No signal* — G (*bound*) 0.492–0.505 over a 32×
  range, H (*crossover*) 0.495–0.512.
- **NSGA-II resolved the earlier `implies_ideal` regression** ([[implies-ideal-regression]]);
  the weighted scheme converges prematurely and is kept only for comparison.

**Why it matters.** The weakening result — a filter improving the metric by being
*disabled*, measured at the worst operating point — motivated the 2026-07-15
crossed-factor campaign above.
