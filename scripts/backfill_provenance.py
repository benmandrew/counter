#!/usr/bin/env python3
"""Write the commit and dirty columns into a results CSV that predates them.

`run_experiments.py` stamps every row with the binary commit that produced it,
but a CSV written before that change has no such column, and the harness
appends to it without one rather than rewriting a file it is sharing with
another host. A campaign that was re-run in stages therefore ends up holding
rows from two binaries with nothing to tell them apart.

This reconstructs the split from the drop backup. `drop_censored_rows.py`
writes `<csv>.bak-<n>rows` before deleting the censored rows, so the rows it
removed -- which are exactly the rows a later stage re-ran -- are the backup's
timed_out=1 keys. Everything else predates the re-run.

    python scripts/backfill_provenance.py \\
        --csv experiments/results-replicate.csv \\
        --backup experiments/results-replicate.csv.bak-1600rows \\
        --old-commit 2d9b890 --new-commit 6576438

Prints what it would do unless --apply is given. Refuses to run if the CSV
already has a commit column, and refuses if any row fails to classify: a row
that is in neither set means the backup does not match the CSV, and guessing
would silently mislabel the provenance this exists to record.
"""

from __future__ import annotations

import argparse
import csv
import shutil
import sys
from pathlib import Path

KEY_FIELDS = ("sweep", "level_name", "selection", "weakening", "metric",
              "repair_mode", "spec", "seed")


def row_key(row: dict[str, str]) -> tuple:
    """Identity of a run, matching how run_experiments.py resumes."""
    return tuple((row.get(f) or "").strip() for f in KEY_FIELDS)


def read_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    csv.field_size_limit(10 ** 7)
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        return list(reader.fieldnames or []), list(reader)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--csv", type=Path, required=True)
    parser.add_argument("--backup", type=Path, required=True,
                        help="the .bak-<n>rows file drop_censored_rows.py wrote")
    parser.add_argument("--old-commit", required=True,
                        help="binary commit for rows that were not re-run")
    parser.add_argument("--new-commit", required=True,
                        help="binary commit for the re-run rows")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    fields, rows = read_rows(args.csv)
    if "commit" in fields:
        sys.exit(f"{args.csv} already has a commit column; nothing to backfill")

    _, backup_rows = read_rows(args.backup)
    rerun_keys = {row_key(r) for r in backup_rows if r.get("timed_out") == "1"}
    kept_keys = {row_key(r) for r in backup_rows if r.get("timed_out") != "1"}
    if not rerun_keys:
        sys.exit(f"{args.backup} holds no timed_out=1 rows; wrong backup file?")

    tally = {args.old_commit: 0, args.new_commit: 0}
    unclassified = []
    for row in rows:
        key = row_key(row)
        if key in rerun_keys:
            row["commit"], row["dirty"] = args.new_commit, "0"
            tally[args.new_commit] += 1
        elif key in kept_keys:
            row["commit"], row["dirty"] = args.old_commit, "0"
            tally[args.old_commit] += 1
        else:
            unclassified.append(key)

    print(f"{len(rows)} rows in {args.csv.name}")
    print(f"  {tally[args.old_commit]:>5} <- {args.old_commit}  (not re-run)")
    print(f"  {tally[args.new_commit]:>5} <- {args.new_commit}  (re-run)")
    if unclassified:
        print(f"\n{len(unclassified)} rows match neither set, e.g.:")
        for key in unclassified[:5]:
            print("   ", dict(zip(KEY_FIELDS, key)))
        sys.exit("Refusing to write: the backup does not describe this CSV.")
    if tally[args.new_commit] != len(rerun_keys):
        sys.exit(f"Expected {len(rerun_keys)} re-run rows, matched "
                 f"{tally[args.new_commit]}: the re-run is incomplete or a key "
                 f"is duplicated. Refusing to write.")

    if not args.apply:
        print("\n(dry run -- pass --apply to rewrite the CSV)")
        return 0

    backup = args.csv.with_suffix(args.csv.suffix + ".bak-pre-provenance")
    shutil.copy2(args.csv, backup)
    print(f"\nbacked up to {backup.name}")
    with open(args.csv, "w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=[*fields, "commit", "dirty"])
        writer.writeheader()
        writer.writerows(rows)
    print(f"rewrote {args.csv.name} with commit and dirty columns")
    return 0


if __name__ == "__main__":
    sys.exit(main())
