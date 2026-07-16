# Experiment Scripts

Scripts for running parameter sweep experiments and analysing the results.

## Prerequisites

- Python 3.11+ (3.10 works but requires the `tomli` backport, installed automatically)
- The `counter` and `compare` binaries built in release mode (see below)

## 1. Build the project

From the repo root:

```sh
cmake --workflow --preset release
```

This produces `build-release/counter` and `build-release/compare`, which the
runner script expects at those exact paths.

## 2. Set up the Python environment

```sh
python3 -m venv .venv
source .venv/bin/activate
pip install -r scripts/requirements.txt
```

Dependencies installed:

| Package | Purpose |
|---|---|
| `pandas` | Data loading and aggregation |
| `scipy` | Kruskal-Wallis and chi-square tests |
| `matplotlib` / `seaborn` | Plotting |
| `scikit-posthocs` | Post-hoc Dunn test |
| `notebook` / `ipykernel` | Running `analyse.ipynb` |
| `tomli` | TOML parsing on Python < 3.11 |

## 3. Generate experiment configs

```sh
python scripts/gen_configs.py
```

Writes one TOML file per (scheme, sweep, level) to
`experiments/configs/<scheme>/`. Safe to re-run — existing files are
overwritten, and regenerating reproduces them byte-for-byte.

The whole grid is generated once per selection scheme, so `selection_scheme` is
a factor of the design rather than a constant. The scheme lives in the
directory because `run_experiments.py` parses (sweep, level) out of the
filename and reads the scheme back off the parent directory.

Every generated config pins `selection_scheme` explicitly. That matters:
`config.hpp` defaults to `weighted`, so a config omitting the key hands the run
to weighted selection without saying so. Check with
`grep -rc nsga2 experiments/configs/nsga2/` if a run's results look unexpectedly
poor.

Sweeps generated, each holding every other parameter at its default:

| Sweep | Parameter varied | Levels |
|---|---|---|
| A | Generations | 5, 10, 15, 20, 30, 40, 60, 80 — `population_size=200` |
| B | Population size | 50, 75, 100, 150, 200, 300, 500, 750, 1000, 1500 — `generations=10` |
| C | Fitness weight presets | default, syntactic-heavy, semantic-heavy, status-only, no-halstead |
| D | `p_trigger` | 0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0 |
| E | `p_response` | 0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0 |
| F | `p_timing` | 0.0, 0.05, 0.15, 0.3, 0.5, 0.75, 1.0 |
| G | `default_bound` | 5, 10, 20, 40, 80, 160 |
| H | `crossover_rate` | 0.0, 0.1, 0.25, 0.5, 0.75, 1.0 |
| I | `mutation_rate` | 0.1, 0.25, 0.5, 0.75, 1.0 |
| J | `run_weakening` | on, off |

63 levels per scheme, 126 configs. Because each sweep holds the others at their
defaults, exactly one level of each is byte-identical to the `A/gen10` baseline
— the aliasing below collapses those nine onto one run per scheme.

`--weakening both` and `--metric both` cross `run_weakening` and
`model_counting.metric` in as factors, nesting them under
`<scheme>/[<wkon|wkoff>/][<direct|log>/]`. The directory label is the short
`direct`/`log`; the TOML value written is the full `direct`/`logarithmic` the
C++ parser accepts. Omitting a flag keeps the flat layout and takes that factor
from the defaults, so a no-arg run reproduces the pre-factor grid.

Two of these are worth a note. `G` is nearly free: the bound enters through the
transfer matrix rather than a SAT call, and bound 160 measures within noise of
bound 5, but it moves the semantic similarity score and so changes which
repairs win. `H`'s `cross0.0` level tests whether crossover contributes at all
— the default of 0.1 leaves the search almost entirely mutation-driven.

## 4. Run the experiments

```sh
python scripts/run_experiments.py                 # the full sweep (default)
python scripts/run_experiments.py --jobs 4        # four runs at a time
```

Runs the selected combinations of (sweep, level, spec, seed) and appends
results to the profile's results CSV. Runs are skipped automatically if they
already appear in the CSV, so the script is safe to interrupt and resume.

All four specs run in every profile — `takeoff`, `fsm`, `fsm-timing`, and
`fsm-combined`.

### Profiles

A profile names one (schemes, sweeps, levels, specs, seeds) selection and the
CSV it writes:

| Profile | Schemes | Sweeps / levels | Seeds | Results CSV | Wall-clock at `--jobs 4` |
|---|---|---|---|---|---|
| `full` (default) | nsga2 | the original 14 levels of A, B, C | 0–29 | `results.csv` | ~29 min (1440 runs) |
| `factorial` | nsga2, weighted | every level of A–J | 0–99 | `results-factorial.csv` | ~14.6 h (43,200 runs) |
| `metric` | nsga2 | C/default only, `metric` crossed direct×log | 0–99 | `results-metric.csv` | ~1 h split across two machines (800 runs at generations=40/population=1000) |
| `tlsf` | nsga2 | A + B, coarse 4-level cross (7 operating points) | 0–59 (ceiling) | `results-tlsf.csv` | ~16 h split across av2+av3 (`--jobs 1`; the six TLSF specs) |

`full` is pinned to the four generation and five population levels it has
always had, so its `results.csv` stays one comparable dataset even though
`gen_configs.py` now emits a finer grid around them. It is 1680 rows from 1440
executions. This README documented 70+ hours for it until `ca46331`, and the
difference is not a tuning win — `black` was timing out on the implication
checks the genetic algorithm generates by the thousand, and now the ones SPOT
folds to a boolean constant are decided without invoking `black` at all.

Measured across two 32-core machines splitting the seeds, each running
`python scripts/run_experiments.py --profile full --jobs 4`: 690 runs in 13.2
minutes and 13.9 minutes. The table assumes `--jobs 4`; `full` defaults to
`--jobs 1`, which serialises the same work into about 115 minutes.

`factorial` is the wide one: 63 levels × 4 specs × 100 seeds × 2 schemes =
50,400 rows from 43,200 executions, about 58 hours of serial compute. Split
across av2 and av3 at `--jobs 4` that is roughly 7.3 hours each. Its point is
that `selection_scheme` becomes a factor rather than a constant, so
nsga2-vs-weighted is answerable at every level instead of only at the baseline
— `results.csv` holds no weighted runs at all, so that comparison cannot be
made from it. Seeds extend cheaply afterwards: `--seeds $(seq -s' ' 100 149)`
against an existing `results-factorial.csv` runs only the new ones.

`metric` isolates the `model_counting.metric` factor — the direct-vs-log choice
of how bounded trace counts become a semantic-similarity score. It crosses that
factor (`direct` × `logarithmic`) over the single all-defaults `C/default` level
at the larger `generations=40`/`population_size=1000` operating point, where
repairs are strong enough for the metric to move outcomes. It reads its own
`experiments/configs-metric/<scheme>/<direct|log>/` grid, so generate it with
`--metric both` (below) before running:

```sh
python scripts/gen_configs.py --generations 40 --population-size 1000 \
    --schemes nsga2 --sweeps C --metric both --out-dir experiments/configs-metric
python scripts/run_experiments.py --profile metric --seeds $(seq -s' ' 0 49) --jobs 4   # av2
python scripts/run_experiments.py --profile metric --seeds $(seq -s' ' 50 99) --jobs 4  # av3
python scripts/merge_experiments.py av2 av3 --profile metric
```

The 800 rows are `2 metrics × 4 specs × 100 seeds` — no sweep grid, so the
budget buys statistical power on the main effect. Ordering is seed-major, so
killing a machine at the wall-clock deadline leaves a balanced design (every
metric/spec sampled at the same seeds, just fewer of them) rather than a
half-finished one.

`tlsf` runs the six basic-TLSF examples (arbiter, gyro-var1, humanoid-531,
lift, lily02, minepump) rather than the four FRETISH specs, swept over
generations (A) and population (B). These specs are far heavier: `ltlsynt`
turns multi-gigabyte resident on them, and three — lift, gyro-var1,
humanoid-531 — take about three minutes each even at the `gen10/pop200`
baseline, against a few seconds for the FRETISH specs. So the grid is a coarse
four-level cross per axis (seven distinct operating points, sharing the
baseline) rather than the fine FRETISH grid: the budget is spent on seeds
instead of gradations. It reads its own `experiments/configs-tlsf/nsga2/` grid,
so generate it with `--tlsf` first:

```sh
python scripts/gen_configs.py --tlsf
python scripts/run_experiments.py --profile tlsf --seeds $(seq -s' ' 0 29)   # av2
python scripts/run_experiments.py --profile tlsf --seeds $(seq -s' ' 30 59)  # av3
python scripts/merge_experiments.py av2 av3 --profile tlsf
```

The seed count is a ceiling, not a commitment: ordering is seed-major, so a
machine killed at its ~16 h deadline leaves a balanced design at whatever seed
depth it reached, and the disjoint 0-29 / 30-59 ranges merge to one balanced
dataset whether or not either finishes.

The profile runs at `--jobs 1`, unlike the FRETISH profiles' `--jobs 4`, for two
reasons. First, `ltlsynt` turns multi-gigabyte resident on these specs, and its
concurrency cap (`runtime.max_concurrent_realizability`) is per counter process,
so one process per machine keeps that cap the machine-wide limit — the 128 GB
av2/av3 grid is generated uncapped (32 cores × ~2.7 GB peaks near 86 GB), but a
smaller-RAM box should pass `--tlsf --max-realizability 6`. Second, `ltlsynt` has
no internal timeout and the genetic search occasionally generates a synthesis
query that runs for minutes; the campaign sets `runtime.ltlsynt_timeout_ms`
(10 s) so such a query is killed and its candidate treated as unrealizable
rather than stalling the run — the count of these appears as `(N timeouts)` in
each run's `ltlsynt` timing row. `compare` decides implies-ideal on TLSF the
same way it does on FRETISH, via the whole-formula implication check, so
`results-tlsf.csv` carries the same columns.

Reduced `quick` and `smoke` profiles existed until that speedup made them
pointless — they saved about 27 minutes between them, at the cost of dropping
`fsm-combined` on the grounds that it never produced a repair. It now finds one
in 328 of 418 runs (78%), so that exclusion was discarding real data.

`--sweeps`, `--specs`, and `--seeds` narrow a profile without defining a new
one: `--specs` and `--seeds` replace the profile's set (so `--specs
fsm-combined` runs that spec alone), and `--sweeps` selects which sweeps run
(so `--sweeps C` runs all five C levels).

### Parallel runs (`--jobs`)

`--jobs N` runs N experiments concurrently (`full` defaults to 1, so `--jobs 4`
is worth passing). The `counter` binary parallelises internally with a thread
pool sized to the machine's core count by default, so when jobs > 1 the
runner caps each run's pool: it writes a derived config
(`<output-dir>/config.toml` — the level's TOML with
`parallel = max(1, cores // jobs)` under `[runtime]`) and passes that to
`counter`. With jobs = 1 the original config file is used unchanged.

### Baseline aliasing

Every sweep holds the other parameters at their defaults, so each sweep's
default level is byte-identical to the `A/gen10` baseline (generations 10,
population 200, default weights): `B/pop200`, `C/default`, `D/ptrig0.5`,
`E/presp0.5`, `F/ptim0.15`, `G/bound20`, `H/cross0.1`, `I/mut1.0` and
`J/weaken-on`. The runner executes the canonical `A/gen10` run once per
(scheme, spec, seed) and emits one CSV row per requested alias — the rows
differ only in `sweep`/`level_name`/`level_value`. Identity is verified
byte-for-byte before aliasing; if the files ever diverge the runner warns and
runs them separately. `--dry-run` tags aliased rows with `(alias of A/gen10)`.

Aliasing never crosses schemes: nsga2's `B/pop200` aliases onto nsga2's
`A/gen10`, never onto weighted's. The byte-identity check enforces that on its
own, since the two configs differ on `selection_scheme`.

### The `n_dropped` column

`counter` drops an individual whose fitness scoring throws — in practice an
external tool failing on one evolved formula — rather than aborting the whole
run, up to `max_scoring_failure_rate` (5%) of a generation. It prints a scoring
report naming the count and reasons, and stays silent when nothing was dropped.

The runner parses that count into `n_dropped`. A run with drops evolved a
thinner population than it asked for, so without the column it is
indistinguishable from a clean one — which is the mistake the report exists to
prevent. Filter on `n_dropped == 0` for a strictly clean dataset.

### The `timed_out` column

Each row records `timed_out` (0/1) so the analysis can distinguish
timeout-censored runs from genuine failures to find a repair. When appending
to a CSV written before this column existed, the runner keeps the file's
original header and drops the column, so legacy files stay consistent.

### Useful flags

```sh
# Preview what would run without executing anything
python scripts/run_experiments.py --dry-run

# Run only specific sweeps
python scripts/run_experiments.py --sweeps A B

# Run only specific specs
python scripts/run_experiments.py --specs takeoff

# Run a small set of seeds for a quick check
python scripts/run_experiments.py --seeds 0 1 2

# Re-run even if results already exist in the CSV
python scripts/run_experiments.py --no-resume

# Combine flags
python scripts/run_experiments.py --sweeps A --specs takeoff --seeds 0 1 2
```

Per-run outputs (repair JSON files) land in `experiments/results/<run-id>/`.

## 5. Splitting across machines

Each run is independent, so the work can be divided on any of the three
filtering axes and recombined afterwards. At ~29 minutes for the full sweep the
coordination rarely pays for itself now, but the seed split below is how the
current `results.csv` was collected.

**By seeds** — recommended, parallelises all sweeps and specs evenly:

```sh
# Machine 1 (seeds 0–14)
python scripts/run_experiments.py --seeds $(seq -s' ' 0 14)

# Machine 2 (seeds 15–29)
python scripts/run_experiments.py --seeds $(seq -s' ' 15 29)
```

**By spec** — useful for isolating `fsm-combined`, the slowest spec at a 6.9s
mean against `fsm`'s 3.3s:

```sh
python scripts/run_experiments.py --specs takeoff fsm            # machine 1
python scripts/run_experiments.py --specs fsm-timing fsm-combined  # machine 2
```

**By sweep** — useful when sweep C is run separately:

```sh
python scripts/run_experiments.py --sweeps A B   # machine 1
python scripts/run_experiments.py --sweeps C     # machine 2
```

### Merging results

`merge_experiments.py` pulls each machine's results and merges them into this
checkout. Configure the machines once in its `REMOTES` dict, then:

```sh
# Pull from every configured remote
python scripts/merge_experiments.py

# Only named remotes
python scripts/merge_experiments.py av2 av3

# Show the rsync plan without transferring or writing
python scripts/merge_experiments.py --dry-run

# Merge another checkout on this machine
python scripts/merge_experiments.py /path/to/counter
```

**`--profile` must match the profile the runs used.** Each profile writes its
own CSV — `full` → `results.csv`, `factorial` → `results-factorial.csv` — and
the flag defaults to `full`. When no source carries a CSV for the chosen
profile, the merge stops and names the mismatch rather than writing anything.

The script rsyncs each remote's `experiments/results/` into the local one, then
merges the CSVs on the natural key
`(sweep, level_name, selection, weakening, metric, spec, seed)`, keeping one row
per key. Local rows win, so a machine's own results are never
overwritten by a remote copy of the same run. Output is sorted by key, which
makes the file byte-stable and the whole operation idempotent — re-running it
never duplicates rows. Per-run directories encode their scheme and seed
(`sweep_A_gen10_nsga2_fsm_seed17`), so they never collide between machines or
between schemes.

`selection`, `weakening` and `metric` are part of the key because a profile may
run every level under both selection schemes (`factorial`), both weakening
states (`cj-large`), or both similarity metrics (`metric`); without them the
crossed rows collapse onto one key and half are dropped in silence. Rows written
before a column existed carry its legacy default — nsga2, wkon, direct — so both
scripts read an absent value as that rather than empty, which keeps resume and
merge working against the older CSVs.

Sources are reached over ssh. An entry in `~/.ssh/config` must match the
hostname as written in `REMOTES` — a bare `Host av3` block does not apply to
`benandrew@av3.cs.man.ac.uk`, so give the block both names and a `HostName`:

```
Host av3 av3.cs.man.ac.uk
	HostName av3.cs.man.ac.uk
	IdentityFile ~/.ssh/id_avlab
	User benandrew
```

## 6. Analyse the results

Launch Jupyter and open the notebook:

```sh
jupyter notebook scripts/analyse.ipynb
```

Or run it non-interactively:

```sh
jupyter nbconvert --to notebook --execute scripts/analyse.ipynb \
    --output scripts/analyse_executed.ipynb
```

The notebook reads `experiments/results.csv` by default; point it at another
results file — an archived or per-machine copy, say — with the `RESULTS_CSV`
env var, which also works for `nbconvert --execute`:

```sh
RESULTS_CSV=../experiments/results-av2.csv jupyter notebook scripts/analyse.ipynb
```

It produces:

- Box plots of best fitness per level
- Bar charts of implies-ideal rate and found-repair rate
- Kruskal-Wallis H test (continuous fitness) with post-hoc Dunn / Holm correction
- Chi-square test (binary implies-ideal) with post-hoc Fisher's exact / Bonferroni correction
- Cross-sweep summary table

Sweep C is displayed only if its results are present in the CSV, which they are
unless the run was narrowed with `--sweeps`.
