#!/usr/bin/env python3
"""Run all parameter sweep experiments and collect metrics to experiments/results.csv.

Usage:
    python scripts/run_experiments.py                   # run everything
    python scripts/run_experiments.py --sweeps A B      # specific sweeps
    python scripts/run_experiments.py --specs takeoff   # specific specs
    python scripts/run_experiments.py --seeds 0 1 2     # specific seeds
    python scripts/run_experiments.py --dry-run         # print plan, no execution
    python scripts/run_experiments.py --no-resume       # ignore existing results
"""

import argparse
import csv
import json
import math
import re
import subprocess
import sys
import time
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
EXPERIMENTS_DIR = REPO_ROOT / "experiments"
CONFIGS_DIR = EXPERIMENTS_DIR / "configs"
RESULTS_DIR = EXPERIMENTS_DIR / "results"
RESULTS_CSV = EXPERIMENTS_DIR / "results.csv"

COUNTER_BIN = REPO_ROOT / "build-release" / "counter"
COMPARE_BIN = REPO_ROOT / "build-release" / "compare"

EXAMPLES_DIR = REPO_ROOT / "examples"
SPECS: dict[str, dict[str, Path]] = {
    "takeoff": {
        "input": EXAMPLES_DIR / "takeoff" / "spec.json",
        "ideals_dir": EXAMPLES_DIR / "takeoff" / "fixes",
    },
    "fsm": {
        "input": EXAMPLES_DIR / "fsm" / "spec.json",
        "ideals_dir": EXAMPLES_DIR / "fsm" / "fixes",
    },
    "fsm-timing": {
        "input": EXAMPLES_DIR / "fsm-timing" / "spec.json",
        "ideals_dir": EXAMPLES_DIR / "fsm-timing" / "fixes",
    },
    "fsm-combined": {
        "input": EXAMPLES_DIR / "fsm-combined" / "spec.json",
        "ideals_dir": EXAMPLES_DIR / "fsm-combined" / "fixes",
    },
}

N_SEEDS = 30
ALL_SEEDS = list(range(N_SEEDS))

CSV_FIELDS = [
    "sweep", "level_name", "level_value", "spec", "seed",
    "found_repair", "n_repairs", "best_fitness",
    "best_relation", "implies_ideal", "n_implies", "wall_time_s",
]

# Higher index = better relation
RELATION_PRIORITY: dict[str, int] = {
    "equivalent":       4,
    "strictly stronger": 3,
    "strictly weaker":  2,
    "incomparable":     1,
    "timeout":          0,
    "none":            -1,
}

SUMMARY_RE = re.compile(
    r"Summary:\s*(\d+) equivalent,\s*(\d+) strictly stronger,"
    r"\s*(\d+) strictly weaker,\s*(\d+) incomparable,\s*(\d+) timeout"
)
PER_REPAIR_RE = re.compile(
    r"^\S.*?\s+:\s+(equivalent|strictly stronger|strictly weaker|incomparable|timeout)"
)


# ── Metadata ─────────────────────────────────────────────────────────────────

def extract_metadata(config_path: Path) -> tuple:
    """Return (sweep, level_name, level_value) from a config filename.

    level_value is the trailing integer if present (e.g. 'gen10' → 10),
    otherwise the level_name string (e.g. 'semantic-heavy').
    """
    stem = config_path.stem  # e.g. 'sweep_A_gen10'
    parts = stem.split("_", 2)  # ['sweep', 'A', 'gen10']
    sweep = parts[1]
    level_name = parts[2]
    m = re.search(r"(\d+)$", level_name)
    level_value: int | str = int(m.group(1)) if m else level_name
    return sweep, level_name, level_value


def counter_timeout(level_name: str, level_value) -> int:
    """Return a generous per-run timeout in seconds."""
    if isinstance(level_value, int) and level_name.startswith("gen"):
        return max(120, level_value * 90)
    if isinstance(level_value, int) and level_name.startswith("pop"):
        return max(120, level_value // 2)
    return 300


# ── Parsing ───────────────────────────────────────────────────────────────────

def parse_repair_files(output_dir: Path) -> tuple[int, float]:
    """Return (n_repairs, best_fitness) from repair_N.json files."""
    files = list(output_dir.glob("repair_*.json"))
    if not files:
        return 0, float("nan")
    best = float("-inf")
    for f in files:
        try:
            data = json.loads(f.read_text())
            total = (data.get("fitness") or {}).get("total")
            if total is not None:
                best = max(best, float(total))
        except Exception:
            pass
    return len(files), best if best != float("-inf") else float("nan")


def tail_line(log_path: Path) -> str:
    """Return the last non-blank line of a log, for console error context.

    counter draws progress with carriage returns, so a raw line may pack many
    updates; keep only the final segment after the last '\\r'.
    """
    try:
        text = log_path.read_text(errors="replace")
    except OSError:
        return ""
    for raw in reversed(text.splitlines()):
        line = raw.split("\r")[-1].strip()
        if line:
            return line
    return ""


def parse_compare_output(stdout: str) -> tuple[str, int, int]:
    """Return (best_relation, implies_ideal, n_implies) from compare stdout.

    implies_ideal is 1 when at least one repair is equivalent or strictly
    stronger than an ideal (i.e. the repair is at least as strong).
    """
    m = SUMMARY_RE.search(stdout)
    if not m:
        return "unknown", 0, 0

    groups = list(map(int, m.groups()))
    n_equiv, n_stronger = groups[0], groups[1]
    n_implies = n_equiv + n_stronger

    best_priority = -1
    best_relation = "timeout"
    for line in stdout.splitlines():
        lm = PER_REPAIR_RE.match(line.strip())
        if lm:
            rel = lm.group(1)
            if RELATION_PRIORITY.get(rel, -1) > best_priority:
                best_priority = RELATION_PRIORITY[rel]
                best_relation = rel

    return best_relation, int(n_implies > 0), n_implies


# ── CSV helpers ───────────────────────────────────────────────────────────────

def load_done_set(csv_path: Path) -> set:
    done: set = set()
    if not csv_path.exists():
        return done
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            done.add((row["sweep"], row["level_name"], row["spec"], int(row["seed"])))
    return done


def append_row(csv_path: Path, row: dict) -> None:
    write_header = not csv_path.exists()
    with open(csv_path, "a", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        if write_header:
            writer.writeheader()
        writer.writerow(row)


# ── Core runner ───────────────────────────────────────────────────────────────

def run_one(config_path: Path, spec_name: str, seed: int) -> dict | None:
    sweep, level_name, level_value = extract_metadata(config_path)
    spec = SPECS[spec_name]
    timeout = counter_timeout(level_name, level_value)

    run_id = f"sweep_{sweep}_{level_name}_{spec_name}_seed{seed:02d}"
    output_dir = RESULTS_DIR / run_id
    output_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(COUNTER_BIN),
        "--input", str(spec["input"]),
        "--output-dir", str(output_dir),
        "--config", str(config_path),
        "--seed", str(seed),
    ]

    log_path = output_dir / "run.log"

    t_start = time.monotonic()
    timed_out = False
    with open(log_path, "wb") as log_file:
        try:
            subprocess.run(
                cmd, check=True, timeout=timeout,
                stdout=log_file, stderr=subprocess.STDOUT,
            )
        except subprocess.TimeoutExpired:
            timed_out = True
            print(f"    TIMEOUT after {timeout}s  (see {log_path})")
        except subprocess.CalledProcessError as e:
            log_file.flush()
            ctx = tail_line(log_path)
            print(f"    ERROR: counter exited {e.returncode}  (see {log_path})"
                  + (f"\n           {ctx}" if ctx else ""))
            return None
    wall = round(time.monotonic() - t_start, 2)

    n_repairs, best_fitness = parse_repair_files(output_dir)

    base = {
        "sweep": sweep,
        "level_name": level_name,
        "level_value": level_value,
        "spec": spec_name,
        "seed": seed,
        "found_repair": int(n_repairs > 0),
        "n_repairs": n_repairs,
        "best_fitness": "" if math.isnan(best_fitness) else round(best_fitness, 6),
        "wall_time_s": wall,
    }

    if n_repairs == 0 or timed_out:
        return {**base, "best_relation": "none", "implies_ideal": 0, "n_implies": 0}

    try:
        result = subprocess.run(
            [str(COMPARE_BIN), "--repairs", str(output_dir),
             "--ideals", str(spec["ideals_dir"])],
            check=True, timeout=120, capture_output=True, text=True,
        )
        best_rel, implies_ideal, n_implies = parse_compare_output(result.stdout)
    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as e:
        print(f"    WARN: compare failed — {e}")
        best_rel, implies_ideal, n_implies = "unknown", 0, 0

    return {**base, "best_relation": best_rel, "implies_ideal": implies_ideal, "n_implies": n_implies}


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sweeps", nargs="+", metavar="SWEEP",
                        help="Sweeps to run (default: all found in experiments/configs/)")
    parser.add_argument("--specs", nargs="+", choices=list(SPECS),
                        default=list(SPECS), metavar="SPEC")
    parser.add_argument("--seeds", nargs="+", type=int, default=ALL_SEEDS, metavar="N")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--no-resume", action="store_true",
                        help="Re-run even if a result already exists in results.csv")
    args = parser.parse_args()

    # Validate binaries
    for bin_path in [COUNTER_BIN, COMPARE_BIN]:
        if not bin_path.exists():
            sys.exit(
                f"Binary not found: {bin_path}\n"
                f"Run: cmake --workflow --preset release"
            )

    # Collect config files
    if not CONFIGS_DIR.exists():
        sys.exit(
            f"No configs found at {CONFIGS_DIR}\n"
            f"Run: python scripts/gen_configs.py"
        )
    def _config_sort_key(p: Path):
        sweep, _, level_value = extract_metadata(p)
        return (sweep, level_value if isinstance(level_value, int) else float("inf"), str(p))

    all_configs = sorted(CONFIGS_DIR.glob("sweep_*.toml"), key=_config_sort_key)
    if args.sweeps:
        wanted = {s.upper() for s in args.sweeps}
        all_configs = [c for c in all_configs if extract_metadata(c)[0] in wanted]
    if not all_configs:
        sys.exit("No matching config files found.")

    done = set() if args.no_resume else load_done_set(RESULTS_CSV)

    # Count total work
    total = len(all_configs) * len(args.specs) * len(args.seeds)
    skipped = sum(
        1 for cfg in all_configs
        for spec in args.specs
        for seed in args.seeds
        if (extract_metadata(cfg)[0], extract_metadata(cfg)[1], spec, seed) in done
    )
    print(f"Runs: {total} total, {skipped} already done, {total - skipped} to execute")
    if args.dry_run:
        for cfg in all_configs:
            sweep, level_name, _ = extract_metadata(cfg)
            for spec in args.specs:
                for seed in args.seeds:
                    tag = "(skip)" if (sweep, level_name, spec, seed) in done else ""
                    print(f"  sweep_{sweep}_{level_name}  spec={spec}  seed={seed:02d}  {tag}")
        return

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    RESULTS_CSV.parent.mkdir(parents=True, exist_ok=True)

    completed = 0
    errors = 0
    t0 = time.monotonic()

    for cfg in all_configs:
        sweep, level_name = extract_metadata(cfg)[:2]
        for spec_name in args.specs:
            for seed in args.seeds:
                if (sweep, level_name, spec_name, seed) in done:
                    continue
                completed += 1
                remaining = total - skipped - completed + 1
                elapsed = time.monotonic() - t0
                eta = (elapsed / completed * remaining) if completed > 1 else 0
                print(
                    f"[{completed}/{total - skipped}]  "
                    f"sweep={sweep}  level={level_name}  spec={spec_name}  seed={seed:02d}"
                    f"  ETA {eta/60:.1f}min",
                    flush=True,
                )
                row = run_one(cfg, spec_name, seed)
                if row is None:
                    errors += 1
                    continue
                append_row(RESULTS_CSV, row)
                done.add((sweep, level_name, spec_name, seed))

    elapsed_total = time.monotonic() - t0
    print(
        f"\nDone. {completed} runs in {elapsed_total/60:.1f} min, {errors} errors."
        f"\nResults: {RESULTS_CSV}"
    )


if __name__ == "__main__":
    main()
