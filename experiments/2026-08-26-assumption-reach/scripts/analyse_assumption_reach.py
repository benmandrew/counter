#!/usr/bin/env python3
"""Analysis for sweep U, the assumption-construction campaign.

Runs the decision rule of `experiments/assumption-reach/PLAN.md` section 5:

  primary    reachoff-s -> reach-s, confined to gyro-var1, gyro-var2 and lift,
             per-run implies_ideal, exact two-sided McNemar at alpha 0.05
  gate       the same contrast over the other 20 families -- a loss there at
             p < 0.05 keeps the keys at their no-op values whatever the
             primary says
  secondary  reach-s -> reachburst-s, reachoff-s -> reachoff-l and
             reach-l -> reachburst-l, plus the section 8 measures

`--complete-seeds` restricts every contrast to the seeds present in all six
arms, which is what makes an interim read over a live campaign balanced rather
than weighted towards whichever arm the runner happened to reach first.

    python3 experiments/assumption-reach/scripts/analyse_assumption_reach.py \
        [--csv PATH] [--complete-seeds]
"""
import argparse
import csv
import math
from collections import defaultdict

CLUSTERS = {
    "arbiter": ["arbiter-aurus", "full-arbiter-aurus", "load-balancer-aurus",
                "prioritized-arbiter-aurus", "round-robin-arbiter-aurus",
                "simple-arbiter-aurus"],
    "lily": ["lily02", "lily11", "lily15", "lily16"],
    "humanoid": ["humanoid-458", "humanoid-503"],
    "ltl2dba": ["ltl2dba-r-2", "ltl2dba-theta-2", "ltl2dba27"],
    "gyro": ["gyro-var1", "gyro-var2"],
    "rg": ["rg1", "rg2"],
    "detector-aurus": ["detector-aurus"],
    "lift": ["lift"],
    "minepump": ["minepump"],
    "pcar-v2-888": ["pcar-v2-888"],
}
TARGET = ("gyro-var1", "gyro-var2", "lift")
ARMS = ("reachoff-s", "reach-s", "reachburst-s",
        "reachoff-l", "reach-l", "reachburst-l")


def truth(s):
    return str(s).strip().lower() in ("1", "true", "yes")


def num(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return float("nan")


def median(xs):
    xs = sorted(x for x in xs if x == x)
    if not xs:
        return float("nan")
    mid = len(xs) // 2
    return xs[mid] if len(xs) % 2 else (xs[mid - 1] + xs[mid]) / 2


def mcnemar_p(gained, lost):
    """Two-sided exact McNemar over the discordant pairs."""
    n = gained + lost
    if n == 0:
        return 1.0
    k = min(gained, lost)
    return min(1.0, 2 * sum(math.comb(n, i) for i in range(k + 1)) / 2 ** n)


def floor_note(n):
    if n == 0:
        return "no discordant pair, so nothing is decidable"
    return (f"p floor 2/2^{n} = {2 / 2 ** n:.4f}"
            + ("" if n >= 6 else "; no p below 0.05 is reachable yet"))


parser = argparse.ArgumentParser()
parser.add_argument("--csv", default="experiments/results-assumption-reach.csv")
parser.add_argument("--complete-cells", action="store_true",
                    help="restrict to (spec, seed) cases present in every arm")
opts = parser.parse_args()

rows = list(csv.DictReader(open(opts.csv)))
arm = defaultdict(dict)
for r in rows:
    arm[r["level_name"]][(r["spec"], int(r["seed"]))] = r

print("=" * 78)
print("INTEGRITY")
print("=" * 78)
print(f"rows {len(rows)}   commits {sorted({r['commit'] for r in rows})}   "
      f"dirty {sorted({r['dirty'] for r in rows})}")
seed_sets = []
for a in ARMS:
    seeds = {k[1] for k in arm[a]}
    seed_sets.append(seeds)
    print(f"  {a:14s} {len(arm[a]):4d} runs, {len({k[0] for k in arm[a]})} "
          f"families x {len(seeds)} seeds {sorted(seeds)}")
complete = sorted(set.intersection(*seed_sets)) if seed_sets else []
print(f"  seeds present in all six arms: {complete}")

# A live campaign leaves the newest seed ragged: the arm the runner reached
# first holds cases the others do not, so a per-arm rate over everything
# present weights the arms differently. Every cell in the intersection is a
# case all six ran, which is what makes the interim table comparable across
# columns as well as within a paired contrast.
cells = set.intersection(*(set(arm[a]) for a in ARMS)) if seed_sets else set()
print(f"  (spec, seed) cases present in all six arms: {len(cells)} of "
      f"{len(set().union(*(set(arm[a]) for a in ARMS)))}")
if opts.complete_cells:
    for a in ARMS:
        arm[a] = {k: v for k, v in arm[a].items() if k in cells}
    print(f"  restricted to complete cells: {len(cells)} per arm")


def contrast(title, before, after, families, note=""):
    keys = sorted(k for k in set(arm[before]) & set(arm[after])
                  if k[0] in families)
    gained = [k for k in keys
              if truth(arm[after][k]["implies_ideal"])
              and not truth(arm[before][k]["implies_ideal"])]
    lost = [k for k in keys
            if truth(arm[before][k]["implies_ideal"])
            and not truth(arm[after][k]["implies_ideal"])]
    both = sum(1 for k in keys if truth(arm[after][k]["implies_ideal"])
               and truth(arm[before][k]["implies_ideal"]))
    p = mcnemar_p(len(gained), len(lost))
    print()
    print("-" * 78)
    print(f"{title}   ({before} -> {after})")
    if note:
        print(f"  {note}")
    print("-" * 78)
    print(f"  {len(keys)} paired runs over {len({k[0] for k in keys})} families")
    print(f"  implies_ideal  {before}: "
          f"{sum(truth(arm[before][k]['implies_ideal']) for k in keys)}/{len(keys)}"
          f"   {after}: "
          f"{sum(truth(arm[after][k]['implies_ideal']) for k in keys)}/{len(keys)}"
          f"   (both {both})")
    print(f"  discordant: {len(gained)} gained, {len(lost)} lost   "
          f"exact McNemar p = {p:.4f}")
    print(f"  {floor_note(len(gained) + len(lost))}")
    for label, ks in (("gained", gained), ("lost", lost)):
        if ks:
            print(f"    {label}: " + ", ".join(f"{s}@{d}" for s, d in ks))
    return p, gained, lost


print()
print("=" * 78)
print("PRIMARY -- section 5, confined to the three target families")
print("=" * 78)
contrast("PRIMARY", "reachoff-s", "reach-s", TARGET)

print()
print("=" * 78)
print("REGRESSION GATE -- the other 20 families")
print("=" * 78)
others = [f for fs in CLUSTERS.values() for f in fs if f not in TARGET]
contrast("GATE", "reachoff-s", "reach-s", others,
         "a loss at p < 0.05 keeps the keys off whatever the primary says")

print()
print("=" * 78)
print("SECONDARY -- no alpha correction, the three contrasts share the corpus")
print("=" * 78)
allf = [f for fs in CLUSTERS.values() for f in fs]
contrast("burst, target families", "reach-s", "reachburst-s", TARGET)
contrast("burst, all families", "reach-s", "reachburst-s", allf)
contrast("search size", "reachoff-s", "reachoff-l", allf)
contrast("burst at size", "reach-l", "reachburst-l", allf)
contrast("operators at size, target", "reachoff-l", "reach-l", TARGET)
contrast("operators at size, all", "reachoff-l", "reach-l", allf)

print()
print("=" * 78)
print("PER-FAMILY implies_ideal (section 6: descriptive, cannot rescue an outcome)")
print("=" * 78)
print(f"{'family':26s}" + "".join(f"{a:>14s}" for a in ARMS))
for cluster, fams in CLUSTERS.items():
    for f in fams:
        cells = []
        for a in ARMS:
            ks = [k for k in arm[a] if k[0] == f]
            hit = sum(truth(arm[a][k]["implies_ideal"]) for k in ks)
            cells.append(f"{hit}/{len(ks)}" if ks else "-")
        line = "".join(f"{c:>14s}" for c in cells)
        mark = " *" if f in TARGET else ""
        print(f"{f + mark:26s}{line}")

print()
print("=" * 78)
print("SECTION 8 MEASURES -- none gating")
print("=" * 78)
print(f"{'arm':14s}{'runs':>7s}{'yield':>10s}{'implies':>10s}"
      f"{'timeout':>10s}{'med wall':>10s}{'med n_rep':>11s}")
for a in ARMS:
    rs = list(arm[a].values())
    if not rs:
        continue
    print(f"{a:14s}{len(rs):7d}"
          f"{sum(truth(r['found_repair']) for r in rs) / len(rs):10.3f}"
          f"{sum(truth(r['implies_ideal']) for r in rs) / len(rs):10.3f}"
          f"{sum(truth(r['timed_out']) for r in rs) / len(rs):10.3f}"
          f"{median(num(r['wall_time_s']) for r in rs):10.1f}"
          f"{median(num(r['n_repairs']) for r in rs):11.1f}")
