#!/usr/bin/env python3
"""End-to-end parity: the same run, configured every way that changes which
libspot path it takes, has to produce the same repairs.

The unit tests compare one call against one tool call. This compares whole runs,
which is where a difference would actually be noticed -- and where it would be
hardest to attribute, because a campaign's numbers moving is not an error
anyone can point at a line for.

Four things are varied, each of which routes work differently:

  simplify_engine   libspot (in process) vs ltlfilt (spawned)
  ltl2tgba timeout  absent (translate inline) vs set (translate on a worker)
  parallel          1 vs 8, which changes the order threads first intern
                    formulae, and so the order SPOT assigns node ids
  repetition        the same configuration twice, which catches anything
                    varying that none of the above explains

The third is the one worth having. SPOT prints commutative operands in node-id
order, ids are assigned on first interning, and the in-process paths share one
intern table for the life of the process -- so unlike a fresh `ltlfilt` per
call, their output depends on what the process did earlier. That is real and
provable (test/runner/differential_tests.cpp), and this script is what says
whether it reaches the results. So far it does not, on any example.

Repairs are compared byte for byte. Nothing here tolerates a difference: two
configurations that disagree are a finding, whichever is right.

Usage:
  scripts/check_engine_parity.py --binary build-release/counter
  scripts/check_engine_parity.py --examples fsm takeoff --generations 10
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Kept small by default. Every example runs once per configuration, so the cost
# is examples x configurations, and the point is agreement rather than search
# quality -- a short run exercises the same code paths as a long one.
CONFIGURATIONS = {
    "libspot": {"simplify_engine": '"libspot"'},
    "ltlfilt": {"simplify_engine": '"ltlfilt"'},
    "libspot-timeout": {
        "simplify_engine": '"libspot"',
        "ltl2tgba_timeout_ms": "30000",
    },
    "libspot-serial": {"simplify_engine": '"libspot"', "parallel": "1"},
    "libspot-wide": {"simplify_engine": '"libspot"', "parallel": "8"},
    # Deliberately identical to the first. A difference here is not about
    # engines at all, and reading it as one would send the search in the wrong
    # direction entirely.
    "libspot-repeat": {"simplify_engine": '"libspot"'},
}


def write_config(path: Path, generations: int, population: int, runtime: dict):
    lines = [
        "[genetic]",
        f"generations = {generations}",
        f"population_size = {population}",
        "",
        "[runtime]",
    ]
    lines += [f"{key} = {value}" for key, value in sorted(runtime.items())]
    path.write_text("\n".join(lines) + "\n")


def digest(output_dir: Path) -> str:
    """One hash over every repair, in a fixed order.

    Named by index rather than content, so sorting by name is sorting by rank
    and a reordering of equally-scored repairs shows up as a difference rather
    than being normalised away.
    """
    sha = hashlib.sha256()
    for repair in sorted(output_dir.glob("repair_*.json")):
        sha.update(repair.name.encode())
        sha.update(repair.read_bytes())
    return sha.hexdigest()[:16]


def run_one(binary: Path, spec: Path, config: Path, seed: int, out: Path):
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    result = subprocess.run(
        [
            str(binary),
            "--input", str(spec),
            "--output-dir", str(out),
            "--config", str(config),
            "--seed", str(seed),
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            f"{binary.name} exited {result.returncode}\n{result.stderr[-2000:]}"
        )
    return digest(out), len(list(out.glob("repair_*.json")))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default="build-release/counter")
    parser.add_argument("--examples", nargs="*",
                        default=["fsm", "fsm-timing", "fsm-combined",
                                 "takeoff"])
    parser.add_argument("--generations", type=int, default=6)
    parser.add_argument("--population", type=int, default=300)
    parser.add_argument("--seed", type=int, default=20260802)
    args = parser.parse_args()

    binary = Path(args.binary)
    if not binary.is_absolute():
        binary = REPO / binary
    if not binary.exists():
        print(f"no binary at {binary}", file=sys.stderr)
        return 2

    failures = 0
    with tempfile.TemporaryDirectory(prefix="engine_parity_") as tmp:
        tmp = Path(tmp)
        configs = {}
        for name, runtime in CONFIGURATIONS.items():
            configs[name] = tmp / f"{name}.toml"
            write_config(configs[name], args.generations, args.population,
                         runtime)

        for example in args.examples:
            # FRETISH examples carry spec.json and TLSF ones spec.tlsf. Both
            # are worth covering: they share the simplification and counting
            # paths, and the TLSF pipeline drives them differently enough that
            # agreement on one says little about the other.
            candidates = [REPO / "examples" / example / name
                          for name in ("spec.json", "spec.tlsf")]
            spec = next((path for path in candidates if path.exists()), None)
            if spec is None:
                print(f"{example}: no spec.json or spec.tlsf, skipped")
                continue
            print(f"{example}:")
            digests = {}
            for name, config in configs.items():
                try:
                    sha, count = run_one(binary, spec, config, args.seed,
                                         tmp / f"{example}-{name}")
                except RuntimeError as exc:
                    print(f"  {name:18} FAILED: {exc}")
                    failures += 1
                    continue
                digests[name] = sha
                print(f"  {name:18} {count:3d} repairs  {sha}")
            distinct = set(digests.values())
            if len(distinct) > 1:
                print(f"  -> DISAGREE: {len(distinct)} distinct results")
                failures += 1
            elif digests:
                print("  -> all configurations agree")

    print()
    print("parity check failed" if failures else "parity check passed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
