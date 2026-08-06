#!/usr/bin/env python3
"""Reproduce every figure in this campaign's PROVENANCE.json from its CSVs.

Reads the merged results CSV and the two per-host well-separation verdict CSVs
that check_well_separated.py wrote, and prints the headline table, the per-spec
leak breakdown and the pre-registered criteria.

    python3 scripts/analyse_wellsep_timing.py --dir experiments/2026-08-06-wellsep-timing

The leak rate is the campaign's gate (PLAN.md criterion 1) and comes from the
verdict files rather than the results CSV: the run cannot be its own witness on
whether its output is well-separated, so every emitted repair is re-checked
outside it with a direct ltlsynt call and no cache.
"""

import argparse
import collections
import csv
import math
import pathlib
import statistics
import sys

ARMS = ("nofilter", "every-gen", "final-only")
BASELINE = "nofilter"


def read_results(directory):
    path = directory / "results-wellsep-timing.csv"
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def read_verdicts(directory):
    rows = []
    for path in sorted(directory.glob("*.well-separation-verdicts.csv")):
        with open(path, newline="") as handle:
            rows.extend(csv.DictReader(handle))
    return rows


def rate(numerator, denominator):
    return float("nan") if denominator == 0 else numerator / denominator


def headline(results, verdicts):
    """Per-arm summary. Wall ratios are paired on (spec, seed), never pooled:
    the specs differ by two orders of magnitude in cost, so a ratio of the
    totals would be a ratio of the expensive specs alone."""
    by_arm = collections.defaultdict(list)
    for row in results:
        by_arm[row["level_name"]].append(row)

    leaks = collections.defaultdict(lambda: [0, 0])
    for row in verdicts:
        counts = leaks[row["arm"]]
        counts[1] += 1
        if row["verdict"] == "not-well-separated":
            counts[0] += 1

    wall = {arm: {(r["spec"], r["seed"]): float(r["wall_time_s"])
                  for r in rows}
            for arm, rows in by_arm.items()}

    print(f"{'arm':<12} {'leak':>8} {'found':>8} {'implies':>8} "
          f"{'repairs':>8} {'wall_s':>8} {'ratio':>8} {'timeouts':>9}")
    for arm in ARMS:
        rows = by_arm[arm]
        bad, total = leaks[arm]
        found = sum(r["found_repair"] == "1" for r in rows)
        implies = sum(r["implies_ideal"] == "1" for r in rows)
        repairs = statistics.mean(int(r["n_repairs"]) for r in rows)
        walls = statistics.mean(float(r["wall_time_s"]) for r in rows)
        timeouts = sum(r["timed_out"] == "1" for r in rows)
        shared = set(wall[arm]) & set(wall[BASELINE])
        ratios = [wall[arm][k] / wall[BASELINE][k]
                  for k in shared if wall[BASELINE][k] > 0]
        print(f"{arm:<12} {rate(bad, total):>8.3f} "
              f"{rate(found, len(rows)):>8.3f} "
              f"{rate(implies, len(rows)):>8.3f} {repairs:>8.2f} "
              f"{walls:>8.2f} {statistics.median(ratios):>8.3f} "
              f"{timeouts:>9d}")
    print(f"\n{sum(t for _, t in leaks.values())} repairs re-checked, "
          f"{sum(1 for r in verdicts if r['verdict'] == 'undecided')} undecided")


def per_spec_leak(verdicts):
    """Criterion 1 broken out. A final-only leak that tracked the unfiltered
    control on a handful of specs and not the rest would be a spec effect; it
    tracks it on all sixteen, which is what makes it a mechanism."""
    table = collections.defaultdict(lambda: collections.defaultdict(lambda: [0, 0]))
    for row in verdicts:
        counts = table[row["spec"]][row["arm"]]
        counts[1] += 1
        if row["verdict"] == "not-well-separated":
            counts[0] += 1
    print(f"\n{'spec':<24}" + "".join(f"{a:>13}" for a in ARMS))
    for spec in sorted(table):
        cells = []
        for arm in ARMS:
            bad, total = table[spec][arm]
            cells.append(f"{rate(bad, total):>12.3f} " if total else f"{'-':>13}")
        print(f"{spec:<24}" + "".join(cells))


def sign_test(pairs):
    """Two-sided sign test on paired ratios. Deliberately not the Wilcoxon the
    plan named: this reruns from the archive with no SciPy dependency, and the
    sign test is the weaker of the two, so a result it calls significant the
    Wilcoxon would too."""
    below = sum(1 for r in pairs if r < 1.0)
    above = sum(1 for r in pairs if r > 1.0)
    n = below + above
    if n == 0:
        return float("nan")
    k = min(below, above)
    tail = sum(math.comb(n, i) for i in range(k + 1))
    return min(1.0, 2 * tail / 2 ** n)


def criteria(results):
    by_key = {(r["level_name"], r["spec"], r["seed"]): r for r in results}
    specs_seeds = {(s, d) for (_, s, d) in by_key}
    print()
    for arm in ("every-gen", "final-only"):
        ratios, wins, losses = [], 0, 0
        for spec, seed in specs_seeds:
            treat = by_key.get((arm, spec, seed))
            base = by_key.get((BASELINE, spec, seed))
            if not treat or not base or float(base["wall_time_s"]) <= 0:
                continue
            ratios.append(float(treat["wall_time_s"]) / float(base["wall_time_s"]))
            wins += treat["implies_ideal"] == "1" and base["implies_ideal"] != "1"
            losses += base["implies_ideal"] == "1" and treat["implies_ideal"] != "1"
        print(f"{arm} vs {BASELINE}: median wall ratio "
              f"{statistics.median(ratios):.3f} (sign test p={sign_test(ratios):.2e}, "
              f"n={len(ratios)}); implies_ideal discordant pairs "
              f"{wins} gained / {losses} lost")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dir", type=pathlib.Path,
                        default=pathlib.Path(__file__).resolve().parent.parent)
    args = parser.parse_args()
    results = read_results(args.dir)
    verdicts = read_verdicts(args.dir)
    headline(results, verdicts)
    per_spec_leak(verdicts)
    criteria(results)
    return 0


if __name__ == "__main__":
    sys.exit(main())
