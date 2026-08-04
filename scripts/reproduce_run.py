#!/usr/bin/env python3
"""Repeats one configuration on one host and reports whether it self-reproduces.

The soak walks a fresh seed every round, which makes it a wide campaign and a
poor experiment: with no configuration ever run twice on one host, a digest
that differs between the two arms cannot be told apart from one that would have
differed against itself. The 24-hour soak ended with exactly that ambiguity on
two `lift` runs.

This closes it. Every repeat is the same example, seed, shape and tier on one
machine, so a difference between repeats is nondeterminism in the run and
nothing else. Read against a second tier on the same host, it separates three
causes that the soak conflated:

  repeats of one tier disagree      the run is nondeterministic on its own, and
                                    a cross-arm difference proves nothing
  each tier self-reproduces but
  the tiers disagree                the configuration changes the answer
  everything agrees                 the soak's difference was the host, most
                                    likely a black or ltlsynt timeout landing
                                    differently under load

Runs are sequential on purpose. These are the largest runs in the corpus and
peak resident set reached 56 GB in the soak, so overlapping them trades the
question being asked for an out-of-memory kill.
"""

import argparse
import csv
import json
import socket
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import soak  # noqa: E402

# The soak's columns with its two campaign-shape fields swapped for the repeat
# index. Derived rather than restated so a new profile counter reaches both
# CSVs at once.
FIELDS = ["repeat"] + [f for f in soak.RUN_FIELDS
                       if f not in ("round", "kind")]
SAMPLE_FIELDS = ["repeat"] + [f for f in soak.SAMPLE_FIELDS
                              if f not in ("round", "kind")]


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="repeat one run configuration")
    ap.add_argument("--binary", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--example", required=True)
    ap.add_argument("--seed", type=int, required=True)
    ap.add_argument("--repeats", type=int, default=5)
    ap.add_argument("--generations", type=int, default=soak.LONG_SHAPE[0])
    ap.add_argument("--population", type=int, default=soak.LONG_SHAPE[1])
    ap.add_argument("--cap", type=int, default=soak.LONG_SHAPE[2])
    # Named to match the soak's tiers so the two sets of digests can be read
    # side by side without a translation table.
    ap.add_argument("--tiers", default="none,loose",
                    help="comma-separated soak tier names to alternate")
    return ap.parse_args()


def tier_timeouts(name: str) -> tuple[int, int]:
    for arm in soak.TIERS.values():
        if name in arm:
            return arm[name]
    raise SystemExit(f"unknown tier {name!r}; "
                     f"known: {sorted(t for a in soak.TIERS.values() for t in a)}")


def main() -> int:
    args = parse_args()
    tiers = [t.strip() for t in args.tiers.split(",") if t.strip()]
    for tier in tiers:
        tier_timeouts(tier)

    args.out.mkdir(parents=True, exist_ok=True)
    work = args.out / "work"
    work.mkdir(exist_ok=True)
    configs = args.out / "configs"
    configs.mkdir(exist_ok=True)

    version = subprocess.run([str(args.binary), "--version"],
                             capture_output=True, text=True).stdout
    (args.out / "manifest.json").write_text(json.dumps({
        "host": socket.gethostname(),
        "binary": str(args.binary),
        "version": version,
        "example": args.example,
        "seed": args.seed,
        "repeats": args.repeats,
        "shape": [args.generations, args.population, args.cap],
        "tiers": {t: tier_timeouts(t) for t in tiers},
        "common_runtime": soak.COMMON_RUNTIME,
    }, indent=2) + "\n")

    for tier in tiers:
        ltl2tgba_ms, simplify_ms = tier_timeouts(tier)
        soak.write_config(configs / f"{tier}.toml", args.generations,
                          args.population, ltl2tgba_ms, simplify_ms)

    runs = (args.out / "runs.csv").open("w", newline="")
    samples = (args.out / "samples.csv").open("w", newline="")
    run_writer = csv.DictWriter(runs, fieldnames=FIELDS, extrasaction="ignore")
    run_writer.writeheader()
    sample_writer = csv.DictWriter(samples, fieldnames=SAMPLE_FIELDS,
                                   extrasaction="ignore")
    sample_writer.writeheader()
    # Flushed before the first run rather than after it. These runs are tens of
    # minutes each, and an empty runs.csv for the first half hour reads as a
    # harness that failed to start.
    runs.flush()
    samples.flush()

    # Tiers alternate inside a repeat rather than running one tier to
    # completion. A machine that drifts -- another job starting, a thermal
    # ceiling -- then drifts across both tiers instead of under one of them.
    for repeat in range(args.repeats):
        for tier in tiers:
            tags = {"repeat": repeat, "tier": tier, "example": args.example}
            row = soak.run_one(args.binary, args.example, args.seed,
                               configs / f"{tier}.toml", args.cap, work,
                               sample_writer, samples.flush, tags)
            row.update(tags)
            row.update({"seed": args.seed, "generations": args.generations,
                        "population": args.population})
            run_writer.writerow(row)
            runs.flush()
            print(f"r{repeat} {tier:6s} {args.example:12s} "
                  f"exit={row['exit_code']:4d} {row['wall_s']:8.1f}s "
                  f"{row['n_repairs']:3d} repairs  {row['digest']}  "
                  f"rss={row['peak_rss_kb']//1024}M", flush=True)

    runs.close()
    samples.close()
    print("reproduce finished", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
