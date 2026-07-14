#!/usr/bin/env python3
"""Pull experiment results from remote machines and merge them into this repo.

Idempotent: run it as many times as you like. Per-run result directories are
named with their seed (``sweep_A_gen10_fsm_seed17``) so they never collide
between machines; rsync only transfers what changed. The aggregate CSV is
merged by its natural key ``(sweep, level_name, spec, seed)``, keeping exactly
one row per key, so re-running never duplicates rows.

``--profile`` selects which CSV to merge, and must match the profile the runs
were executed under: ``run_experiments.py --profile quick`` writes
``results-quick.csv``, so merging those results needs ``--profile quick`` too.

Configure the remote machines in REMOTES below, then:

    python scripts/merge_experiments.py                  # pull from all REMOTES (full)
    python scripts/merge_experiments.py --profile quick  # merge results-quick.csv
    python scripts/merge_experiments.py av2 av3          # only named remotes
    python scripts/merge_experiments.py --dry-run        # show rsync plan, no writes
    python scripts/merge_experiments.py /path/to/copy    # merge an already-rsynced dir

A source may be a remote (``host:/path/to/counter``) or a local path to another
counter checkout. Bare hostnames use REMOTE_ROOT as the repo path.
"""

import argparse
import csv
import re
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

RESULTS_DIR = REPO_ROOT / "experiments" / "results"

# Mirrors the per-profile results_csv names in run_experiments.py's PROFILES.
# Adding a profile there means adding it here too.
PROFILE_CSVS: dict[str, str] = {
    "full": "results.csv",
    "quick": "results-quick.csv",
    "smoke": "results-smoke.csv",
}

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


def safe_stem(label: str) -> str:
    """Filename-safe stem for a source label.

    A label may be an absolute path or a ``host:/path`` spec. Joining one onto
    tmp_dir unsanitised escapes it entirely — ``Path("/tmp/x") / "/a/b.csv"``
    is ``/a/b.csv``, since an absolute right-hand side discards the base. The
    whole label is folded (rather than just its basename) so that two sources
    sharing a basename stay distinct.
    """
    stem = re.sub(r"[^A-Za-z0-9._-]+", "_", label).strip("_")
    # Keep filenames well inside the usual 255-byte limit; the tail is the
    # discriminating part of a path, so keep that end.
    if len(stem) > 100:
        stem = stem[-100:].lstrip("_")
    return stem or "source"


def run_rsync(src: str, dst: str, dry_run: bool) -> None:
    cmd = ["rsync", "-av"]
    if dry_run:
        cmd.append("--dry-run")
    cmd += [src, dst]
    print(f"  $ {' '.join(cmd)}")
    subprocess.run(cmd, check=True)


def pull_source(
    label: str, root: str, dry_run: bool, tmp_dir: Path, csv_name: str
) -> Path | None:
    """rsync one source's results/ and its profile CSV locally.

    Returns the local path of the pulled CSV (under tmp_dir), or None if that
    machine had no CSV for this profile.
    """
    print(f"[{label}] {root}")
    # Trailing slash on the source dir: copy contents into results/, merging.
    run_rsync(f"{root}/experiments/results/", f"{RESULTS_DIR}/", dry_run)

    csv_local = tmp_dir / f"{safe_stem(label)}.{csv_name}"
    try:
        run_rsync(f"{root}/experiments/{csv_name}", str(csv_local), dry_run)
    except subprocess.CalledProcessError:
        print(f"  (no {csv_name} on {label}, skipping its rows)")
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


def merge_csv(pulled: list[Path], results_csv: Path) -> None:
    """Merge pulled CSVs into results_csv, one row per key. Local rows win.

    Ordering is deterministic (sorted by key), so the file is byte-stable across
    repeated runs once every source has been merged in.
    """
    header, local_rows = read_rows(results_csv)

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

    results_csv.parent.mkdir(parents=True, exist_ok=True)
    with open(results_csv, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=header)
        writer.writeheader()
        for k in sorted(merged, key=sort_key):
            writer.writerow(merged[k])

    print(
        f"\nMerged CSV: {len(merged)} rows total "
        f"({len(local_rows)} local, {added} new from remotes) → {results_csv}"
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
    parser.add_argument("--profile", choices=list(PROFILE_CSVS), default="full",
                        help="Which profile's CSV to merge; must match the profile "
                             "the runs used (default: full).")
    parser.add_argument("--dry-run", action="store_true",
                        help="Show the rsync commands without transferring or writing.")
    args = parser.parse_args()

    csv_name = PROFILE_CSVS[args.profile]
    results_csv = REPO_ROOT / "experiments" / csv_name
    print(f"Profile: {args.profile} → {csv_name}\n")

    names = args.sources or list(REMOTES)
    tmp_dir = REPO_ROOT / "experiments" / ".merge_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    pulled: list[Path] = []
    for name in names:
        label, root = resolve_source(name)
        csv_path = pull_source(label, root, args.dry_run, tmp_dir, csv_name)
        if csv_path is not None:
            pulled.append(csv_path)

    if args.dry_run:
        print("\nDry run — no files written. CSV merge skipped.")
        return

    if not pulled:
        print(f"\nNo {csv_name} found on any source — nothing merged. "
              f"Did those runs use --profile {args.profile}?")
        return

    merge_csv(pulled, results_csv)


if __name__ == "__main__":
    main()
