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
re-run — existing files are overwritten.

Sweeps generated:

| Sweep | Parameter varied | Levels |
|---|---|---|
| A | Generations | 5, 10, 20, 40 — `population_size=200` |
| B | Population size | 50, 100, 200, 500, 1000 — `generations=10` |
| C | Fitness weight presets | default, syntactic-heavy, semantic-heavy, status-only, no-halstead |

## 4. Run the experiments

```sh
python scripts/run_experiments.py
```

Runs every combination of (sweep, level, spec, seed) and appends results to
`experiments/results.csv`. Runs are skipped automatically if they already
appear in the CSV, so the script is safe to interrupt and resume.

Specs tested by default: `takeoff`, `arbiter`. Each runs 30 seeds (0–29).

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
filtering axes and recombined afterwards.

**By seeds** — recommended, parallelises all sweeps and specs evenly:

```sh
# Machine 1 (seeds 0–14)
python scripts/run_experiments.py --seeds $(seq -s' ' 0 14)

# Machine 2 (seeds 15–29)
python scripts/run_experiments.py --seeds $(seq -s' ' 15 29)
```

**By spec** — simplest if the two specs have similar runtimes:

```sh
python scripts/run_experiments.py --specs takeoff   # machine 1
python scripts/run_experiments.py --specs arbiter   # machine 2
```

**By sweep** — useful when sweep C is run separately:

```sh
python scripts/run_experiments.py --sweeps A B   # machine 1
python scripts/run_experiments.py --sweeps C     # machine 2
```

### Merging results

Copy the remote machine's `experiments/results.csv` locally (e.g. as
`machine2_results.csv`), then append its data rows:

```sh
tail -n +2 machine2_results.csv >> experiments/results.csv
```

If there is any risk that both machines ran overlapping (sweep, level, spec,
seed) combinations, deduplicate before merging:

```sh
(head -1 experiments/results.csv; \
 tail -n +2 experiments/results.csv machine2_results.csv | sort -u) \
    > merged.csv && mv merged.csv experiments/results.csv
```

The per-run repair files under `experiments/results/` can be merged the same
way — just `rsync` or copy the directories across; directory names encode the
full run identity so there are no collisions as long as the seed split was clean.

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

The notebook reads `experiments/results.csv` and produces:

- Box plots of best fitness per level
- Bar charts of implies-ideal rate and found-repair rate
- Kruskal-Wallis H test (continuous fitness) with post-hoc Dunn / Holm correction
- Chi-square test (binary implies-ideal) with post-hoc Fisher's exact / Bonferroni correction
- Cross-sweep summary table

Sweep C is optional and only displayed if its results are present in the CSV.
