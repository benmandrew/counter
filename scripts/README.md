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

Writes one TOML file per (sweep, level) to `experiments/configs/`. Safe to
re-run — existing files are overwritten, and regenerating reproduces them
byte-for-byte.

Every generated config pins `selection_scheme = "nsga2"`. That is a deliberate
departure from `config.hpp`, which defaults to `weighted`: the sweeps are run
under NSGA-II, and a config omitting the key hands the runs back to weighted
selection without saying so. Check with `grep -c nsga2 experiments/configs/*.toml`
if a run's results look unexpectedly poor.

Sweeps generated:

| Sweep | Parameter varied | Levels |
|---|---|---|
| A | Generations | 5, 10, 20, 40 — `population_size=200` |
| B | Population size | 50, 100, 200, 500, 1000 — `generations=10` |
| C | Fitness weight presets | default, syntactic-heavy, semantic-heavy, status-only, no-halstead |

## 4. Run the experiments

```sh
python scripts/run_experiments.py                 # the full sweep (default)
python scripts/run_experiments.py --jobs 4        # four runs at a time
```

Runs the selected combinations of (sweep, level, spec, seed) and appends
results to the profile's results CSV. Runs are skipped automatically if they
already appear in the CSV, so the script is safe to interrupt and resume.

All four specs run — `takeoff`, `fsm`, `fsm-timing`, and `fsm-combined` — on
seeds 0–29.

### Profiles

A profile names one (sweeps, levels, specs, seeds) selection and the CSV it
writes. `full` is the only one, and the default:

| Profile | Sweeps / levels | Specs | Seeds | Results CSV | Wall-clock at `--jobs 4` |
|---|---|---|---|---|---|
| `full` | all levels of A, B, C | all 4 | 0–29 | `results.csv` | ~29 min (1440 runs) |

The sweep is 3 sweeps totalling 14 levels × 4 specs × 30 seeds: 1680 rows from
1440 executions, once the baseline aliasing below is accounted for. It finishes
in roughly 29 minutes at `--jobs 4` on 32 cores. This README documented 70+
hours until `ca46331`, and the difference is not a tuning win — `black` was
timing out on the implication checks the genetic algorithm generates by the
thousand, and now the ones SPOT folds to a boolean constant are decided without
invoking `black` at all.

Measured across two 32-core machines splitting the seeds, each running
`python scripts/run_experiments.py --profile full --jobs 4`: 690 runs in 13.2
minutes and 13.9 minutes. The table assumes `--jobs 4`; `full` defaults to
`--jobs 1`, which serialises the same work into about 115 minutes.

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

Three configs are byte-identical (generations 10, population 200, default
weights): sweep A `gen10`, sweep B `pop200`, and sweep C `default`. The
runner executes the canonical `A/gen10` run once per (spec, seed) and emits
one CSV row per requested alias — the rows differ only in
`sweep`/`level_name`/`level_value`. Identity is verified byte-for-byte before
aliasing; if the files ever diverge the runner warns and runs them
separately. `--dry-run` tags aliased rows with `(alias of A/gen10)`.

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
own CSV — `full` → `results.csv` — and the flag defaults to `full`. When no
source carries a CSV for the chosen profile, the merge stops and names the
mismatch rather than writing anything.

The script rsyncs each remote's `experiments/results/` into the local one, then
merges the CSVs on the natural key `(sweep, level_name, spec, seed)`, keeping one
row per key. Local rows win, so a machine's own results are never overwritten by
a remote copy of the same run. Output is sorted by key, which makes the file
byte-stable and the whole operation idempotent — re-running it never duplicates
rows. Per-run directories encode their seed (`sweep_A_gen10_fsm_seed17`), so they
never collide between machines.

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
