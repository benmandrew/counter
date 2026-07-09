#!/usr/bin/env python3
"""Recompute the compare-derived columns in experiments/results.csv.

Re-runs `compare` against the repair JSONs already on disk and rewrites only
best_relation / implies_ideal / n_implies for each row — best_fitness,
n_repairs, wall_time_s, etc. are left untouched. No `counter` re-runs.

This fixes rows whose comparison was recorded as `unknown` because compare
failed at collection time (e.g. the pre-`--ideals` interface, or a compare
subprocess timeout).

Usage:
    python scripts/recompare.py              # only rows with best_relation=unknown
    python scripts/recompare.py --all        # every row that has repairs on disk
    python scripts/recompare.py --dry-run    # list what would be recomputed

Run this AFTER run_experiments.py has finished — it rewrites results.csv and
will race the sweep's row appends if run concurrently.
"""

import argparse
import csv
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from run_experiments import (  # noqa: E402
    COMPARE_BIN,
    COMPARE_TIMEOUT_S,
    CSV_FIELDS,
    EXPERIMENTS_DIR,
    SPECS,
    parse_compare_output,
)


def recompare_dir(output_dir: Path, ideals_dir: Path):
    """Return (best_relation, implies_ideal, n_implies) or None on failure."""
    try:
        result = subprocess.run(
            [str(COMPARE_BIN), "--repairs", str(output_dir),
             "--ideals", str(ideals_dir)],
            check=True, timeout=COMPARE_TIMEOUT_S, capture_output=True, text=True,
        )
    except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as e:
        print(f"      WARN: compare failed — {e}")
        return None
    return parse_compare_output(result.stdout)


def write_csv_atomic(path: Path, rows: list) -> None:
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    tmp.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", type=Path, default=EXPERIMENTS_DIR,
                        help="directory holding results.csv and results/ "
                             "(default: the repo's experiments/); point at a "
                             "per-machine copy, e.g. results-av2")
    parser.add_argument("--all", action="store_true",
                        help="recompute every row with repairs, not just "
                             "best_relation=unknown")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    results_csv = args.base / "results.csv"
    results_dir = args.base / "results"

    if not COMPARE_BIN.exists():
        sys.exit(f"compare binary not found: {COMPARE_BIN}")
    if not results_csv.exists():
        sys.exit(f"No results at {results_csv}")

    with open(results_csv, newline="") as f:
        rows = list(csv.DictReader(f))

    targets = []
    for i, row in enumerate(rows):
        if int(row["n_repairs"] or 0) <= 0:
            continue  # nothing to compare
        if not args.all and row["best_relation"] != "unknown":
            continue
        targets.append(i)

    print(f"{len(targets)} rows to recompute (of {len(rows)} total)")
    if args.dry_run:
        for i in targets:
            r = rows[i]
            print(f"  sweep_{r['sweep']}_{r['level_name']}_{r['spec']}"
                  f"_seed{int(r['seed']):02d}  ({r['n_repairs']} repairs, "
                  f"now={r['best_relation']})")
        return

    updated = failed = missing = 0
    for n, i in enumerate(targets, 1):
        row = rows[i]
        spec = row["spec"]
        if spec not in SPECS:
            print(f"  [{n}/{len(targets)}] skip: spec '{spec}' not in SPECS")
            continue
        run_id = (f"sweep_{row['sweep']}_{row['level_name']}_{spec}"
                  f"_seed{int(row['seed']):02d}")
        output_dir = results_dir / run_id
        if not list(output_dir.glob("repair_*.json")):
            print(f"  [{n}/{len(targets)}] {run_id}: no repair files, skipping")
            missing += 1
            continue

        print(f"  [{n}/{len(targets)}] {run_id}  ({row['n_repairs']} repairs)",
              flush=True)
        res = recompare_dir(output_dir, SPECS[spec]["ideals_dir"])
        if res is None:
            failed += 1
            continue
        best_rel, implies_ideal, n_implies = res
        row["best_relation"] = best_rel
        row["implies_ideal"] = implies_ideal
        row["n_implies"] = n_implies
        updated += 1
        print(f"        -> {best_rel}  (implies_ideal={implies_ideal})")
        write_csv_atomic(results_csv, rows)  # persist after each row

    print(f"\nDone. {updated} updated, {failed} failed, {missing} missing "
          f"repairs.\nResults: {results_csv}")


if __name__ == "__main__":
    main()
