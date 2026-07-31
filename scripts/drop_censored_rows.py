#!/usr/bin/env python3
"""Drop timed-out rows from a results CSV so a re-run at a looser cap replaces
them.

`run_experiments.py` resumes by CSV key and never cleans output directories, so
the way to re-run a subset is to delete its rows and let resume select what is
missing. This does that for rows the per-run timeout censored, optionally
restricted to a spec and a seed range, and removes the matching run directories
so the re-run does not inherit a half-written one.

Writes a timestamped backup of the CSV beside it before touching anything, and
prints what it would remove unless --apply is given.

    python scripts/drop_censored_rows.py --csv experiments/results-replicate.csv \
        --results-dir experiments/results-replicate --specs takeoff fsm-combined
    python scripts/drop_censored_rows.py --csv experiments/results-replicate.csv \
        --results-dir experiments/results-replicate --specs fsm-timing \
        --seeds 0-24 --apply
"""

from __future__ import annotations

import argparse
import csv
import shutil
import sys
from pathlib import Path


def parse_seeds(spec: str) -> set[int]:
    seeds: set[int] = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            lo, hi = part.split("-", 1)
            seeds.update(range(int(lo), int(hi) + 1))
        else:
            seeds.add(int(part))
    return seeds


def run_dir_names(row: dict[str, str]) -> list[str]:
    """Run directory names that could hold this row's output.

    Mirrors run_id in run_experiments.py:
        sweep_{sweep}_{level}_{scheme}[_{weakening}]_{spec}_seed{seed:02d}
    The weakening tag is present only for profiles that cross the factor, and
    the CSV cannot tell which profile wrote a row, so both spellings are
    returned and only the ones that exist are removed.
    """
    head = f"sweep_{row['sweep']}_{row['level_name']}_{row['selection']}"
    tail = f"{row['spec']}_seed{int(row['seed']):02d}"
    names = [f"{head}_{tail}"]
    weakening = (row.get("weakening") or "").strip()
    if weakening:
        names.append(f"{head}_{weakening}_{tail}")
    return names


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Drop timed-out rows so a looser-cap re-run replaces them")
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--results-dir", type=Path, required=True)
    parser.add_argument("--specs", nargs="*", default=None,
                        help="Only drop rows for these specs (default: all)")
    parser.add_argument("--selection", nargs="*", default=None,
                        help="Only drop rows for these selection schemes")
    parser.add_argument("--seeds", default=None,
                        help="Seed subset, e.g. '0-24' or '0,5,7' "
                             "(default: all seeds)")
    parser.add_argument("--apply", action="store_true",
                        help="Actually rewrite the CSV and delete directories")
    args = parser.parse_args()

    if not args.csv.is_file():
        print(f"error: no such CSV: {args.csv}", file=sys.stderr)
        return 1

    seeds = parse_seeds(args.seeds) if args.seeds else None

    with args.csv.open() as handle:
        reader = csv.DictReader(handle)
        fieldnames = reader.fieldnames or []
        rows = list(reader)

    if "timed_out" not in fieldnames:
        print("error: CSV has no timed_out column", file=sys.stderr)
        return 1

    def censored(row: dict[str, str]) -> bool:
        if (row.get("timed_out") or "0").strip() not in ("1", "true", "True"):
            return False
        if args.specs and row["spec"] not in args.specs:
            return False
        if args.selection and row["selection"] not in args.selection:
            return False
        if seeds is not None and int(row["seed"]) not in seeds:
            return False
        return True

    drop = [row for row in rows if censored(row)]
    keep = [row for row in rows if not censored(row)]

    by_cell: dict[tuple[str, str], int] = {}
    for row in drop:
        key = (row["spec"], row["selection"])
        by_cell[key] = by_cell.get(key, 0) + 1

    print(f"{len(rows)} rows in {args.csv.name}; "
          f"{len(drop)} censored rows selected, {len(keep)} kept")
    for (spec_name, selection), count in sorted(by_cell.items()):
        print(f"  {spec_name:<14} {selection:<17} {count:>4}")

    victims = [args.results_dir / name
               for row in drop for name in run_dir_names(row)]
    present = [path for path in victims if path.is_dir()]
    print(f"{len(present)} run directories to remove")

    if not args.apply:
        print("\n(dry run — pass --apply to rewrite the CSV and delete "
              "directories)")
        return 0

    if not drop:
        print("nothing to do")
        return 0

    backup = args.csv.with_suffix(f".csv.bak-{len(rows)}rows")
    shutil.copy2(args.csv, backup)
    print(f"backed up to {backup.name}")

    tmp = args.csv.with_suffix(".csv.tmp")
    with tmp.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(keep)
    tmp.replace(args.csv)
    print(f"rewrote {args.csv.name} with {len(keep)} rows")

    for path in present:
        shutil.rmtree(path)
    print(f"removed {len(present)} run directories")
    return 0


if __name__ == "__main__":
    sys.exit(main())
