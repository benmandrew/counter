# PLAN — nsga2 vs nsga2-replicate

Campaign plan for the selection-scheme comparison. The `Nsga2Replicate` scheme
landed in `4f139d3` behind an opt-in config key; this campaign decides whether it
becomes the default. All runs execute on av2 and av3 (32 cores, 125 GB RAM).

## 1. What the existing evidence cannot settle

The scheme's own A/B — four FRETISH specs, `gen10/pop200`, `elitism_rate = 0`,
20 seeds — reported distinct candidates per generation rising from 5.9 to 98.3
and `n_repairs` from 3.43 to 9.94. Three gaps keep that from deciding the
default.

**No quality headroom.** Pooled `implies_ideal` was 0.500 in both arms. Measured
per-spec at `gen40/pop1000` (`experiments/2026-07-16-metric`): takeoff 1.000,
fsm-timing 1.000, fsm 0.530, fsm-combined 0.075. Two of four specs sit at the
ceiling, where no scheme can move them.

**The elitism setting was not the shipped one.** `config.hpp` defaults
`elitism_rate` to 0.1, and no sweep has ever varied it. Elitism carries the top
fraction over verbatim, re-injecting exact duplicates — the mechanism replicate
exists to undo. The gain measured at 0 may shrink at 0.1.

**Compute was not controlled.** Replicate costs roughly 50% more wall time, so
"better" is currently confounded with "spent more".

## 2. Design

Paired by seed, blocked by spec, three arms:

| Arm | Scheme | Config |
|---|---|---|
| A control | `nsga2` | sweep R, `gen40/pop1000` |
| B treatment | `nsga2-replicate` | sweep R, `gen40/pop1000` |
| C compute-matched control | `nsga2` | sweep S, generations scaled by the measured cost ratio |

Each arm crosses `elitism_rate ∈ {0, 0.1}` (sweep R's two levels). The design is
the config tree rather than the profile table: sweep S is generated under `nsga2`
only, so both schemes and both sweeps still yield three arms, not four.

**Phase 1 — FRETISH**, `gen40/pop1000`, 4 specs × 3 arms × 2 elitism levels ×
200 seeds = 4800 runs. The operating point matches the cj-large and metric
campaigns, so arm A is checkable against 19,644 existing rows instead of being
taken on trust. fsm carries the power as the only family measured off both
bounds; fsm-combined tests upward movement; takeoff and fsm-timing are
saturation controls that should not move.

**Phase 2 — TLSF**, `gen10/pop200`, 5 specs × 2 arms × 2 elitism levels × 60
seeds = 1200 runs. Specs chosen by measured headroom from the 2026-07-21 muc
campaign: arbiter (`found_repair` 0.50) and lily02 (`implies_ideal` 0.670) can
move; gyro-var1, lift and minepump sit at `implies_ideal` 0.000 with
`found_repair` near 1.0 and serve as negative controls. humanoid-531 is excluded
on cost alone (965 s mean, 2400 s p90).

**Weakening cross** (`replicate-wkoff`), 2 specs × 2 arms × 2 elitism levels ×
200 seeds = 1600 runs. Phase 1 runs at the shipped `run_weakening = true`, where
the filter drops about 70% of each generation's offspring — so it measures
replicate in the configuration that discards most of what breeding produces, and
cj-large found that turning the filter off raises `implies_ideal`. This arm
repeats sweep R with the filter off on fsm and fsm-combined, the two specs with
headroom. Arm C is not crossed in: compute-matching is a claim about the
scheme's cost, inherited from the `wkon` arm.

**Resolution.** `implies_ideal` is a per-run binary. 200 pairs per cell resolves
about 0.15 absolute at fsm's p ≈ 0.5, about 0.10 pooled over the two elitism
levels. An 0.05 effect would need roughly 1600 seeds per arm and is out of reach.

## 3. Prep (landed on this branch)

1. `nsga2-replicate` added to `gen_configs.py`'s `SCHEMES`. `scheme_of()` reads
   any non-factor directory name, so `run_experiments.py` needed no change.
2. `elitism_rate` added to `DEFAULTS` and `make_toml`, emitted only when a sweep
   overrides it. Sweep R crosses it; sweep S is R at a scaled generation budget,
   with the multiplier on `--compute-match-factor`. Verified byte-identical
   output for sweeps A–J against the pre-change generator.
3. Profiles `replicate` and `replicate-tlsf`, each with its own configs
   directory, results directory and CSV, registered in `merge_experiments.py`.
4. Compare-timeout bias fixed. `compare`'s cost scales with
   `n_repairs × n_ideals`, and the treatment arm produces about three times the
   repairs, so the old behaviour — record `implies_ideal = 0` on timeout — scored
   a slower comparison as a worse repair and biased the response against the arm
   under test. The timeout is now per-profile (1800 s for `replicate`) and lands
   in its own `compare_timed_out` column for the analysis to exclude.
5. Stage records carry `distinct`: how many of a stage's survivors are distinct
   specifications. A two-generation run on takeoff reports `order-parents` at
   `n_out=20, distinct=1`, and `restore-elites` at `n_out=4, distinct=2` — the
   seeding and elitism duplication of #39, visible for the first time from
   normal run output.

A latent bug turned up next door: `gen_configs.py --tlsf` computed a TLSF output
directory and then wrote to `args.out_dir` regardless. Every past TLSF campaign
passed `--out-dir` explicitly, so it never fired.

## 3a. The per-run timeout censored the treatment arm

Sweep R's caps — 900 s, 1500 s for fsm-combined — were sized off nsga2's costs.
Replicate runs several times longer, so the cap fired on the treatment arm
alone. Over the first 1422 complete seed-pairs:

| spec | cap | nsga2 timeouts | replicate timeouts |
|---|---|---|---|
| fsm | 900 s | 0 | 0 |
| takeoff | 900 s | 0 | 34 / 354 |
| fsm-combined | 1500 s | 0 | 56 / 358 |
| fsm-timing | 900 s | 0 | 250 / 352 |

This is the same bias the `compare_timeout` fix addressed, one level up and
missed at design time: a cap that binds on one arm and never on the other is a
one-sided filter on the response under test, since a censored run records
`found_repair = 0` and `implies_ideal = 0`.

It manufactures a decisive result. Pooled McNemar over all pairs reads 111
replicate wins to 388, p ≈ 5e-37. Restricted to pairs where neither arm timed
out it is 111 to 104, p = 0.68. On fsm-timing and takeoff every uncensored pair
scores 1.000 in both arms with not one discordant pair — the entire apparent
loss is the clock. fsm-timing's replicate median wall time *is* the cap, so that
cell has no recoverable central tendency.

The repair keeps every valid measurement. Only replicate rows are damaged, and
only the censored ones: `scripts/drop_censored_rows.py` deletes those rows and
their run directories, and the `replicate-recap` profile — sweep R at a 3600 s
cap, sharing the same CSV and results directory — re-runs exactly what resume
then finds missing. fsm is absent from it because nothing censored there.
fsm-timing runs on a 50-seed subsample: it scores 1.000 in both arms wherever it
completes, so its re-run buys an honest wall-time ratio rather than a quality
contrast, at about 6 h per machine against 31 h for the full 250.

That comes to about 78 runs per machine — 17 takeoff, ~28 fsm-combined, ~33
fsm-timing — which is 10 h at four jobs if they average half the new cap and
19 h if they all reach it. The three stages are chained rather than
co-scheduled, sweep R then wkoff then recap, because `wall_time_s` is a
response here and two profiles sharing a machine would contaminate it.

Recap rows are indistinguishable from originals by key, deliberately — the
analysis wants one row per cell — but recoverable from the data, since a
surviving row censored at the old cap still reads `timed_out = 1` at
`wall_time_s` of exactly 900 or 1500. Within fsm-timing, the un-resampled cells
stay censored at 900 s; the analysis must read that spec's wall-time ratio from
the subsample alone.

`replicate-wkoff` was raised to 3600 s on both specs before it launched, so the
weakening cross never inherited the defect.

## 4. Launch

Calibrate first. The 1.5 default for `--compute-match-factor` is extrapolated
from a `gen10/pop200` measurement and sizes arm C; replace it with the measured
ratio before generating sweep S. Calibrate with jobs far exceeding workers —
timing 4 jobs on 4 workers underestimated a prior campaign's cost by about 27%.

```sh
# Phase 1 configs (arms A and B, then arm C at the measured ratio)
python scripts/gen_configs.py --schemes nsga2 nsga2-replicate --sweeps R \
    --generations 40 --population-size 1000 --out-dir experiments/configs-replicate
python scripts/gen_configs.py --schemes nsga2 --sweeps S \
    --generations 40 --population-size 1000 --compute-match-factor <measured> \
    --out-dir experiments/configs-replicate

python scripts/run_experiments.py --profile replicate --seeds $(seq -s' ' 0 99)    # av2
python scripts/run_experiments.py --profile replicate --seeds $(seq -s' ' 100 199) # av3
python scripts/merge_experiments.py av2 av3 --profile replicate

# Weakening cross, after sweep R finishes (it shares R's CSV and machines)
python scripts/gen_configs.py --schemes nsga2 nsga2-replicate --sweeps R \
    --weakening off --generations 40 --population-size 1000 \
    --out-dir experiments/configs-replicate

python scripts/run_experiments.py --profile replicate-wkoff --seeds $(seq -s' ' 0 99)    # av2
python scripts/run_experiments.py --profile replicate-wkoff --seeds $(seq -s' ' 100 199) # av3

# Recap: replace the rows the original caps censored (see 3a). Drop first, then
# let resume select what is missing. --apply is required to write anything.
python scripts/drop_censored_rows.py --csv experiments/results-replicate.csv \
    --results-dir experiments/results-replicate \
    --specs takeoff fsm-combined --apply
python scripts/drop_censored_rows.py --csv experiments/results-replicate.csv \
    --results-dir experiments/results-replicate \
    --specs fsm-timing --seeds 0-24 --apply     # av2; 100-124 on av3

python scripts/run_experiments.py --profile replicate-recap --seeds $(seq -s' ' 0 99)    # av2
python scripts/run_experiments.py --profile replicate-recap --seeds $(seq -s' ' 100 199) # av3

# Phase 2 configs and runs
python scripts/gen_configs.py --tlsf --schemes nsga2 nsga2-replicate --sweeps R \
    --out-dir experiments/configs-replicate-tlsf

python scripts/run_experiments.py --profile replicate-tlsf --seeds $(seq -s' ' 0 29)  # av2
python scripts/run_experiments.py --profile replicate-tlsf --seeds $(seq -s' ' 30 59) # av3
python scripts/merge_experiments.py av2 av3 --profile replicate-tlsf
```

The phases must not overlap on one machine. `wall_time_s` is a response here —
it is how arm C is judged — so co-scheduling would contaminate the measurement
even though the profiles write to separate directories. Budget about 7 h for
phase 1 and 5 h for phase 2 per machine, plus roughly an hour of calibration:
one overnight run, at ±30% until calibration replaces the extrapolation.

For the mechanism check rather than the outcome, generate a small grid with
`--dashboard` and run a 10-seed subset. Each run then writes `progress.jsonl`
with per-stage `distinct` counts, which is what shows whether replicate keeps
the diversity it claims to keep. It costs a flushed write per stage, so leave it
off for the campaign proper.

## 5. Analysis and decision rule

Pre-registered, so the analysis cannot be steered by its own result. McNemar's
exact test on discordant seed-pairs per spec; Cochran–Mantel–Haenszel stratified
by spec for the pooled effect; paired wall-time ratios by Wilcoxon signed-rank.
Rows with `compare_timed_out = 1` are excluded, and their count is reported.

`nsga2-replicate` becomes the `config.hpp` default only if all four hold: the
pooled `implies_ideal` confidence interval has a lower bound above −0.02; it
beats arm C, not merely arm A; `found_repair` and `n_repairs` improve; and the
median paired wall-time ratio is at most 2.0. Otherwise it stays opt-in, and #39
stays open on its two remaining levers — single-point seeding and the weakening
filter's 70.4% drop rate.

## 6. Hygiene

Fresh `results-replicate.csv` and results directory: resume skips by CSV key and
never cleans output directories, so a stale row survives an engine change.
Verify `build-release/counter`'s mtime post-dates `4f139d3` on both machines
before launching — a stale binary fakes failures that are already fixed. Seed
ranges are disjoint and seed-major, so a deadline kill leaves a balanced design.
Launch detached and guard on `ps comm` rather than `pgrep -f`. Run the ltl2tgba
orphan janitor during the TLSF phase.

The campaign is deliberately narrow: one factor, one confound, one control arm.
It will not say whether the `1 / (1 + rank)` weighting is the right one, only
whether the scheme that uses it earns the default.
