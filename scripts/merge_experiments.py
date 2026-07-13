#!/usr/bin/env python3
"""Pull experiment results from remote machines and merge them into this repo.

Idempotent: run it as many times as you like. Per-run result directories are
named with their seed (``sweep_A_gen10_fsm_seed17``) so they never collide
between machines; rsync only transfers what changed. The aggregate
``results.csv`` is merged by its natural key ``(sweep, level_name, spec, seed)``,
keeping exactly one row per key, so re-running never duplicates rows.

Configure the remote machines in REMOTES below, then:

    python scripts/merge_experiments.py                  # pull from all REMOTES
    python scripts/merge_experiments.py av2 av3          # only named remotes
    python scripts/merge_experiments.py --dry-run        # show rsync plan, no writes
    python scripts/merge_experiments.py /path/to/copy    # merge an already-rsynced dir

A source may be a remote (``host:/path/to/counter``) or a local path to another
counter checkout. Bare hostnames use REMOTE_ROOT as the repo path.
"""

import argparse
import csv
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent

# ── Configure your machines here ──────────────────────────────────────────────
# Each entry is an ssh destination as accepted by ssh/rsync. A bare host uses
# REMOTE_ROOT as the repo path; append ":/custom/path/to/counter" to override.
REMOTES: dict[str, str] = {
    "av2": "benandrew@av2.cs.man.ac.uk",
    "av3": "benandrew@av3.cs.man.ac.uk",
}

# Path to the counter checkout on a remote when only a host is given.
REMOTE_ROOT = "/home/benandrew/projects/counter"
# ──────────────────────────────────────────────────────────────────────────────

RESULTS_CSV = REPO_ROOT / "experiments" / "results.csv"
RESULTS_DIR = REPO_ROOT / "experiments" / "results"

# Natural key of a results.csv row: one run per (sweep, level_name, spec, seed).
KEY_FIELDS = ("sweep", "level_name", "spec", "seed")


def resolve_source(name: str) -> tuple[str, str]:
    """Map a CLI argument to (label, rsync_root).

    ``name`` may be a key in REMOTES, a raw ``host:/path`` / ``host`` spec, or a
    local filesystem path to another counter checkout. A ``host`` with no path
    component gets REMOTE_ROOT appended.
    """
    if name in REMOTES:
        # A configured entry is always an ssh destination — a bare host (no
        # colon) or an explicit host:path. Append REMOTE_ROOT when no path given.
        host, sep, path = REMOTES[name].partition(":")
        root = f"{host}:{path}" if sep else f"{REMOTES[name]}:{REMOTE_ROOT}"
        return name, root
    # A raw argument: an existing local path is another checkout to merge;
    # anything else is treated as a remote spec (host or host:path).
    local = Path(name).expanduser()
    if local.exists():
        return name, str(local)
    host, sep, _ = name.partition(":")
    root = name if sep else f"{host}:{REMOTE_ROOT}"
    return name, root


def run_rsync(src: str, dst: str, dry_run: bool) -> None:
    cmd = ["rsync", "-av"]
    if dry_run:
        cmd.append("--dry-run")
    cmd += [src, dst]
    print(f"  $ {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def pull_source(label: str, root: str, dry_run: bool, tmp_dir: Path) -> Path | None:
    """rsync one source's results/ and results.csv locally.

    Returns the local path of the pulled CSV (under tmp_dir), or None if that
    machine had no results.csv.
    """
    print(f"[{label}] {root}")
    # Trailing slash on the source dir: copy contents into results/, merging.
    run_rsync(f"{root}/experiments/results/", f"{RESULTS_DIR}/", dry_run)

    csv_local = tmp_dir / f"{label}.results.csv"
    try:
        run_rsync(f"{root}/experiments/results.csv", str(csv_local), dry_run)
    except subprocess.CalledProcessError:
        print(f"  (no results.csv on {label}, skipping its rows)")
        return None
    return csv_local if csv_local.exists() else None


def read_rows(path: Path) -> tuple[list[str], list[dict]]:
    if not path.exists():
        return [], []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        return list(reader.fieldnames or []), list(reader)


def key_of(row: dict) -> tuple:
    return tuple(row.get(f, "") for f in KEY_FIELDS)


def merge_csv(pulled: list[Path]) -> None:
    """Merge pulled CSVs into RESULTS_CSV, one row per key. Local rows win.

    Ordering is deterministic (sorted by key), so the file is byte-stable across
    repeated runs once every source has been merged in.
    """
    header, local_rows = read_rows(RESULTS_CSV)

    merged: dict[tuple, dict] = {}
    # Local first so existing rows take precedence over remote copies of the
    # same key; remotes only contribute keys we don't already have.
    for row in local_rows:
        merged[key_of(row)] = row
    added = 0
    for csv_path in pulled:
        remote_header, remote_rows = read_rows(csv_path)
        if not header:
            header = remote_header
        for row in remote_rows:
            k = key_of(row)
            if k not in merged:
                merged[k] = row
                added += 1

    if not header:
        print("No CSV data found anywhere — nothing to merge.")
        return

    def sort_key(k: tuple):
        sweep, level_name, spec, seed = k
        try:
            seed_v: object = int(seed)
        except (TypeError, ValueError):
            seed_v = seed
        return (sweep, level_name, spec, seed_v)

    RESULTS_CSV.parent.mkdir(parents=True, exist_ok=True)
    with open(RESULTS_CSV, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        for k in sorted(merged, key=sort_key):
            writer.writerow(merged[k])

    print(
        f"\nMerged CSV: {len(merged)} rows total "
        f"({len(local_rows)} local, {added} new from remotes) → {RESULTS_CSV}"
    )
    seeds = sorted({int(r["seed"]) for r in merged.values() if r.get("seed", "").isdigit()})
    if seeds:
        print(f"Seeds present: {min(seeds)}–{max(seeds)} ({len(seeds)} distinct)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "sources", nargs="*", metavar="SOURCE",
        help="Remotes to pull (names from REMOTES, host:/path specs, or local "
             "paths). Default: all entries in REMOTES.",
    )
    parser.add_argument("--dry-run", action="store_true",
                        help="Show the rsync commands without transferring or writing.")
    args = parser.parse_args()

    names = args.sources or list(REMOTES)
    tmp_dir = REPO_ROOT / "experiments" / ".merge_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    pulled: list[Path] = []
    for name in names:
        label, root = resolve_source(name)
        csv_path = pull_source(label, root, args.dry_run, tmp_dir)
        if csv_path is not None:
            pulled.append(csv_path)

    if args.dry_run:
        print("\nDry run — no files written. CSV merge skipped.")
        return

    merge_csv(pulled)


if __name__ == "__main__":
    main()
