#!/usr/bin/env python3
"""Prices the in-process simplifier against spawning `ltlfilt` for the same work.

The soak measured the in-process path against itself at four deadline tiers and
never against the process boundary it replaced, so it can say the path is
harmless and cannot say what it buys. The profile counters do not close that
either: they record `.calls` and `.max_wall_ms` per site, so an avoided exec is
counted but never totalled, and a call count is not a saving.

This runs the same example, seed and shape under `simplify_engine = "libspot"`
and `= "ltlfilt"` and reads the wall clock. Both deadlines are 0, so nothing
fires and nothing moves to a worker: `libspot` simplifies inline under the
process-wide lock, `ltlfilt` spawns per call, and that difference is the whole
of what separates the two arms.

Two things it deliberately does not measure. Translation has no engine key --
`run_ltl2tgba_for_counting` is in process either way -- so the boundary priced
here is simplification alone, which is about 85% of libspot calls but not all
of them. And the lock's 8ms fallback means `libspot` still spawns whenever the
lock is contended, so this is the *achieved* saving at this `parallel`, not the
saving that removing the boundary would give if the lock were free.

Engines alternate inside a seed rather than running one arm to completion. A
machine that drifts then drifts across both arms instead of under one of them,
which is the failure that makes two batches of runs unreadable.
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

ENGINES = ("libspot", "ltlfilt")

# soak's columns with its campaign-shape fields swapped for the engine. Derived
# rather than restated so a new profile counter reaches both CSVs at once.
FIELDS = ["engine", "repeat"] + [f for f in soak.RUN_FIELDS
                                 if f not in ("round", "kind", "tier")]
SAMPLE_FIELDS = ["engine", "repeat"] + [f for f in soak.SAMPLE_FIELDS
                                        if f not in ("round", "kind", "tier")]


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="price in-process vs fork+exec")
    ap.add_argument("--binary", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--examples", default="",
                    help="comma-separated; default is every example found")
    ap.add_argument("--seeds", type=int, default=6,
                    help="consecutive seeds from --base-seed")
    ap.add_argument("--base-seed", type=int, default=20260804)
    ap.add_argument("--generations", type=int, default=soak.SHORT_SHAPE[0])
    ap.add_argument("--population", type=int, default=soak.SHORT_SHAPE[1])
    ap.add_argument("--cap", type=int, default=soak.SHORT_SHAPE[2])
    return ap.parse_args()


def write_config(path: Path, generations: int, population: int,
                 engine: str) -> None:
    """soak.write_config with the engine pinned and both deadlines at zero.

    Zero is written rather than omitted for the same reason soak writes it: the
    file is then a full record of the arm instead of something that has to be
    read against the binary's defaults.
    """
    runtime = dict(soak.COMMON_RUNTIME)
    runtime["ltl2tgba_timeout_ms"] = "0"
    runtime["simplify_timeout_ms"] = "0"
    runtime["simplify_engine"] = f'"{engine}"'
    lines = ["[genetic]", f"generations = {generations}",
             f"population_size = {population}", "", "[runtime]"]
    lines += [f"{key} = {value}" for key, value in sorted(runtime.items())]
    path.write_text("\n".join(lines) + "\n")


def main() -> int:
    args = parse_args()
    names = ([e.strip() for e in args.examples.split(",") if e.strip()]
             or soak.examples())

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
        "question": "what does simplifying in process buy over spawning "
                    "ltlfilt, at this parallel and on this corpus",
        "examples": names,
        "seeds": [args.base_seed + i for i in range(args.seeds)],
        "shape": [args.generations, args.population, args.cap],
        "engines": list(ENGINES),
        "deadlines": {"ltl2tgba_timeout_ms": 0, "simplify_timeout_ms": 0,
                      "why": "neither fires, so no work moves to a worker and "
                             "the arms differ in the engine key alone"},
        "common_runtime": soak.COMMON_RUNTIME,
    }, indent=2) + "\n")

    for engine in ENGINES:
        write_config(configs / f"{engine}.toml", args.generations,
                     args.population, engine)

    runs = (args.out / "runs.csv").open("w", newline="")
    samples = (args.out / "samples.csv").open("w", newline="")
    run_writer = csv.DictWriter(runs, fieldnames=FIELDS, extrasaction="ignore")
    run_writer.writeheader()
    sample_writer = csv.DictWriter(samples, fieldnames=SAMPLE_FIELDS,
                                   extrasaction="ignore")
    sample_writer.writeheader()
    runs.flush()
    samples.flush()

    total = args.seeds * len(names) * len(ENGINES)
    done = 0
    for repeat in range(args.seeds):
        seed = args.base_seed + repeat
        for example in names:
            for engine in ENGINES:
                tags = {"engine": engine, "repeat": repeat,
                        "example": example}
                row = soak.run_one(args.binary, example, seed,
                                   configs / f"{engine}.toml", args.cap, work,
                                   sample_writer, samples.flush, tags)
                row.update(tags)
                row.update({"seed": seed, "generations": args.generations,
                            "population": args.population})
                run_writer.writerow(row)
                runs.flush()
                done += 1
                print(f"[{done}/{total}] s{seed} {engine:8s} "
                      f"{example:16s} exit={row['exit_code']:4d} "
                      f"{row['wall_s']:8.1f}s {row['n_repairs']:3d} repairs  "
                      f"{row['digest']}  rss={row['peak_rss_kb']//1024}M",
                      flush=True)

    runs.close()
    samples.close()
    print("compare finished", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
