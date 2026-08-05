#!/usr/bin/env python3
"""Reads the in-process vs fork+exec comparison written by compare_engines.py.

The campaign pairs every (host, seed, example) under `simplify_engine =
"libspot"` and `= "ltlfilt"`, so every comparison here is within-host and
within-seed. Pass one --dir per host; they are concatenated, since the hosts
carry disjoint seeds.

Three questions, in the order they decide anything. Do the arms agree? If they
disagree the speed is irrelevant. What does the boundary cost in wall time,
totalled over the campaign rather than averaged per run -- a mean over runs
flatters the in-process arm, because the runs it cannot speed up are the slow
ones that hit the cap in both arms. And what does it cost in memory, which is
the resource that moved out of a killable child and into counter itself.

Pairs where either side hit the wall cap are excluded from the correctness and
wall-ratio figures: a capped run dies on SIGKILL, writes no repairs and hashes
to the empty digest, so comparing it measures the cap. They are kept in the
resource figures, where the peak is real regardless of how the run ended.
"""

import argparse
import csv
import statistics
from pathlib import Path

ENGINES = ("libspot", "ltlfilt")
EMPTY_DIGEST = "e3b0c44298fc1c14"


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(description="read the engine comparison")
    ap.add_argument("--dir", type=Path, action="append", required=True,
                    help="a host's output directory; repeat per host")
    return ap.parse_args()


def load(dirs) -> list:
    rows = []
    for d in dirs:
        for row in csv.DictReader((d / "runs.csv").open()):
            row["host"] = d.name
            rows.append(row)
    return rows


def pair_up(rows) -> list:
    """Group into (host, seed, example) pairs, dropping any incomplete one."""
    by_key = {}
    for row in rows:
        key = (row["host"], row["seed"], row["example"])
        by_key.setdefault(key, {})[row["engine"]] = row
    return [(k, v) for k, v in sorted(by_key.items())
            if all(e in v for e in ENGINES)]


def number(value) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return 0.0


def capped(pair) -> bool:
    return any(pair[e]["timed_out"] == "1" for e in ENGINES)


def quantile(values, fraction) -> float:
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(fraction * len(ordered)))]


def report_correctness(pairs) -> None:
    live = [p for _, p in pairs if not capped(p)]
    agree = [p for p in live
             if p["libspot"]["digest"] == p["ltlfilt"]["digest"]]
    print(f"correctness: {len(agree)}/{len(live)} uncapped pairs agree")
    for pair in live:
        if pair["libspot"]["digest"] != pair["ltlfilt"]["digest"]:
            print(f"   MISMATCH {pair['libspot']['example']:22s} "
                  f"seed {pair['libspot']['seed']}  "
                  f"libspot {pair['libspot']['digest']} "
                  f"({pair['libspot']['n_repairs']} repairs)  "
                  f"ltlfilt {pair['ltlfilt']['digest']} "
                  f"({pair['ltlfilt']['n_repairs']} repairs)")
    empty = sum(1 for p in live
                if p["libspot"]["digest"].startswith(EMPTY_DIGEST))
    if empty:
        print(f"   note: {empty} uncapped pairs still hash to the empty "
              f"digest, so they agree on having found nothing")


def report_wall(pairs) -> None:
    live = [p for _, p in pairs if not capped(p)]
    ratios = [number(p["ltlfilt"]["wall_s"]) / number(p["libspot"]["wall_s"])
              for p in live if number(p["libspot"]["wall_s"]) > 0.5]
    print(f"\nwall, ltlfilt/libspot over {len(ratios)} uncapped pairs "
          f"longer than 0.5 s")
    print(f"   median {statistics.median(ratios):.3f}   "
          f"p10 {quantile(ratios, 0.1):.3f}   "
          f"p90 {quantile(ratios, 0.9):.3f}")
    for label, subset in (("uncapped only", live),
                          ("every pair", [p for _, p in pairs])):
        lib = sum(number(p["libspot"]["wall_s"]) for p in subset)
        ltl = sum(number(p["ltlfilt"]["wall_s"]) for p in subset)
        saving = 100 * (1 - lib / ltl) if ltl else 0.0
        print(f"   total wall, {label:14s}: libspot {lib / 3600:5.2f} h   "
              f"ltlfilt {ltl / 3600:5.2f} h   in-process saves {saving:4.1f}%")
    done = {e: sum(1 for _, p in pairs if p[e]["timed_out"] != "1")
            for e in ENGINES}
    print(f"   completed within the cap: libspot {done['libspot']}/"
          f"{len(pairs)}   ltlfilt {done['ltlfilt']}/{len(pairs)}")


def report_resources(pairs) -> None:
    def peak(row):
        return number(row["peak_rss_kb"]) / 1048576

    peaks = {e: [peak(p[e]) for _, p in pairs] for e in ENGINES}
    print(f"\npeak resident set, GB, over all {len(pairs)} pairs")
    print(f"   {'':10s}      libspot   ltlfilt")
    for label, fraction in (("median", 0.5), ("p90", 0.9), ("p99", 0.99)):
        print(f"   {label:10s} {quantile(peaks['libspot'], fraction):9.2f} "
              f"{quantile(peaks['ltlfilt'], fraction):9.2f}")
    print(f"   {'max':10s} {max(peaks['libspot']):9.2f} "
          f"{max(peaks['ltlfilt']):9.2f}")
    doubled = sum(1 for _, p in pairs
                  if peak(p["ltlfilt"]) > 0
                  and peak(p["libspot"]) > 2 * peak(p["ltlfilt"]))
    print(f"   pairs where libspot held over twice ltlfilt's peak: "
          f"{doubled}/{len(pairs)}")
    heavy = [(k, p) for k, p in pairs
             if peak(p["libspot"]) > 2 or peak(p["ltlfilt"]) > 2]
    for key, pair in sorted(heavy, key=lambda kp: -peak(kp[1]["libspot"])):
        print(f"      {key[0]} {key[1]} {key[2]:22s} "
              f"libspot {peak(pair['libspot']):6.2f} GB   "
              f"ltlfilt {peak(pair['ltlfilt']):5.2f} GB   "
              f"capped {pair['libspot']['timed_out']}/"
              f"{pair['ltlfilt']['timed_out']}")


def report_boundary(pairs) -> None:
    """How much of the boundary the lock actually removed.

    libspot still spawns whenever the process-wide lock is not free inside its
    budget, so the arm is not exec-free and the saving above is the achieved
    one at this parallel, not the one a free lock would give.
    """
    calls = sum(number(p["libspot"]["ltlfilt.libspot-simplify.calls"])
                for _, p in pairs)
    busy = sum(number(p["libspot"]["simplify-lock-busy"]) for _, p in pairs)
    execs = {e: sum(number(p[e]["proc.fork+exec.calls"]) for _, p in pairs)
             for e in ENGINES}
    share = 100 * busy / calls if calls else 0.0
    print(f"\nboundary: {calls:.0f} simplify calls in the libspot arm, "
          f"{busy:.0f} found the lock busy and spawned anyway "
          f"({share:.1f}%)")
    print(f"   fork+exec calls: libspot arm {execs['libspot']:.0f}   "
          f"ltlfilt arm {execs['ltlfilt']:.0f}")


def main() -> int:
    dirs = parse_args().dir
    rows = load(dirs)
    pairs = pair_up(rows)
    print(f"{len(rows)} runs from {len(dirs)} hosts, "
          f"{len(pairs)} complete pairs, "
          f"{sum(1 for _, p in pairs if capped(p))} with a capped side")
    report_correctness(pairs)
    report_wall(pairs)
    report_resources(pairs)
    report_boundary(pairs)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
