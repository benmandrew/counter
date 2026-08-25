#!/usr/bin/env python3
"""Pre-registered analysis for the 2026-08-23 monotone-operator campaign.

Runs the decision rule of `experiments/2026-08-23-monotone/PLAN.md` section 5:

  primary    monooff -> monoon, the two new mutation probabilities together,
             per-family implies_ideal rate over 20 seeds, two-sided exact
             Wilcoxon signed-rank over families at alpha 0.05
  clustered  the same test over the 10 correlated-family clusters -- the read
             the outcome rests on where the two disagree
  secondary  the other three contrasts and the per-arm measures, none of which
             can move the outcome

The control arm is another campaign's archive, by design: sweep T has no
fourth arm, and PLAN.md section 3 pairs against the 2026-08-21-aurus-h2h-ship
rows on (spec, seed). Point --control at that directory; the default is the
sibling archive.

Written after the campaign to read the merged CSVs, so this is not a vendored
driver copy -- it ran nothing.

    python3 scripts/analyse_monotone.py [campaign-dir] [--control DIR]
"""
import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

# PLAN.md section 7. The same 10 problem types the head-to-head used: variants
# within a cluster differ mostly in parameter count and can be expected to
# succeed or fail together, so 25 families are about 10 independent problems.
CLUSTERS = {
    "arbiter": ["arbiter-aurus", "full-arbiter-aurus", "load-balancer-aurus",
                "prioritized-arbiter-aurus", "round-robin-arbiter-aurus",
                "simple-arbiter-aurus"],
    "lily": ["lily02", "lily11", "lily15", "lily16"],
    "humanoid": ["humanoid-458", "humanoid-503", "humanoid-531",
                 "humanoid-742"],
    "ltl2dba": ["ltl2dba-r-2", "ltl2dba-theta-2", "ltl2dba27"],
    "gyro": ["gyro-var1", "gyro-var2"],
    "rg": ["rg1", "rg2"],
    "detector-aurus": ["detector-aurus"],
    "lift": ["lift"],
    "minepump": ["minepump"],
    "pcar-v2-888": ["pcar-v2-888"],
}

ARMS = ("monooff", "monoon", "monoship")


def truth(s):
    return str(s).strip().lower() in ("1", "true", "yes")


def num(s):
    try:
        return float(s)
    except (TypeError, ValueError):
        return float("nan")


# -- Wilcoxon signed-rank ------------------------------------------------------

def signed_rank(diffs):
    """``(w_plus, ranks)`` over the non-zero differences, ties averaged."""
    nz = [d for d in diffs if d != 0]
    order = sorted(range(len(nz)), key=lambda i: abs(nz[i]))
    ranks = [0.0] * len(nz)
    i = 0
    while i < len(order):
        j = i
        while j + 1 < len(order) and abs(nz[order[j + 1]]) == abs(nz[order[i]]):
            j += 1
        avg = (i + j) / 2 + 1
        for k in range(i, j + 1):
            ranks[order[k]] = avg
        i = j + 1
    w_plus = sum(r for r, d in zip(ranks, nz) if d > 0)
    return w_plus, ranks


def wilcoxon_p(diffs):
    """Two-sided exact p, by enumerating every sign assignment of the ranks.

    A randomisation test conditional on the observed ranks, which stays valid
    when averaged ties make them fractional -- where the textbook table, built
    on distinct integer ranks, no longer applies. Rates here are k/20, so tied
    absolute differences are the common case rather than the exception.
    """
    w, ranks = signed_rank(diffs)
    n = len(ranks)
    if n == 0:
        return 1.0, 0.0, 0
    dist = {0: 1}
    for r in (int(round(x * 2)) for x in ranks):
        nxt = defaultdict(int)
        for s, c in dist.items():
            nxt[s] += c
            nxt[s + r] += c
        dist = nxt
    total = 2 ** n
    w2 = int(round(w * 2))
    le = sum(c for s, c in dist.items() if s <= w2)
    ge = sum(c for s, c in dist.items() if s >= w2)
    return min(1.0, 2 * min(le, ge) / total), w, n


def mcnemar_p(gained, lost):
    """Two-sided exact McNemar over the discordant pairs."""
    n = gained + lost
    if n == 0:
        return 1.0
    k = min(gained, lost)
    return min(1.0, 2 * sum(math.comb(n, i) for i in range(k + 1)) / 2 ** n)


def power_floor(n, unit):
    floor = 2 / 2 ** n if n else 1.0
    line = (f"  n = {n} non-tied {unit}: exact two-sided p floor "
            f"2/2^{n} = {floor:.4f}")
    if n < 6:
        return line + "; no p below 0.05 is reachable, so the read is uninformative"
    return line


# -- load ----------------------------------------------------------------------

parser = argparse.ArgumentParser()
parser.add_argument("campaign_dir", nargs="?",
                    default="experiments/2026-08-23-monotone")
parser.add_argument("--control", default=None,
                    help="archive holding the paired control arm")
opts = parser.parse_args()

DIR = Path(opts.campaign_dir)
CONTROL = Path(opts.control) if opts.control else \
    DIR.parent / "2026-08-21-aurus-h2h-ship"

rows = list(csv.DictReader(open(DIR / "results-monotone.csv")))
arm = defaultdict(dict)
for r in rows:
    arm[r["level_name"]][(r["spec"], r["seed"])] = r

control = {}
control_csv = CONTROL / "results-aurus-h2h.csv"
if control_csv.exists():
    for r in csv.DictReader(open(control_csv)):
        control[(r["spec"], r["seed"])] = r
else:
    print(f"note: no control arm at {control_csv}; "
          f"the two contrasts against it are skipped", file=sys.stderr)

print("=" * 78)
print("INTEGRITY")
print("=" * 78)
print(f"rows {len(rows)}   commits {sorted({r['commit'] for r in rows})}   "
      f"dirty {sorted({r['dirty'] for r in rows})}")
for a in ARMS:
    print(f"  {a:9s} {len(arm[a]):4d} runs, {len({k[0] for k in arm[a]})} "
          f"families x {len({k[1] for k in arm[a]})} seeds")
if control:
    print(f"  control   {len(control):4d} runs (from {CONTROL.name})")


# -- one contrast --------------------------------------------------------------

def rates(runs, keys):
    hit, n = defaultdict(int), defaultdict(int)
    for k in keys:
        n[k[0]] += 1
        hit[k[0]] += truth(runs[k]["implies_ideal"])
    return {f: hit[f] / n[f] for f in n}


def contrast(title, before, after, primary=False):
    keys = sorted(set(before) & set(after))
    if not keys:
        print(f"\n{title}: no paired runs, skipped")
        return
    ra, rb = rates(before, keys), rates(after, keys)
    fams = sorted(ra)
    diffs = [rb[f] - ra[f] for f in fams]
    p, w, n = wilcoxon_p(diffs)

    cl = []
    for name, members in CLUSTERS.items():
        present = [f for f in members if f in ra]
        if present:
            cl.append((statistics.mean(ra[f] for f in present),
                       statistics.mean(rb[f] for f in present)))
    cld = [b - a for a, b in cl]
    cp, cw, cn = wilcoxon_p(cld)

    gained = sum(1 for k in keys if truth(after[k]["implies_ideal"])
                 and not truth(before[k]["implies_ideal"]))
    lost = sum(1 for k in keys if truth(before[k]["implies_ideal"])
               and not truth(after[k]["implies_ideal"]))

    print()
    print("=" * 78)
    print(f"{'PRIMARY -- ' if primary else 'SECONDARY -- '}{title}")
    print("=" * 78)
    print(f"  {len(keys)} paired (spec, seed) runs")
    print(f"  per family : {statistics.mean(ra.values()):.3f} -> "
          f"{statistics.mean(rb.values()):.3f}   "
          f"+{sum(d > 0 for d in diffs)} / -{sum(d < 0 for d in diffs)} / "
          f"={sum(d == 0 for d in diffs)}   W+ = {w:g}, p = {p:.4f}")
    print(power_floor(n, "families"))
    print(f"  clustered  : {statistics.mean(a for a, _ in cl):.3f} -> "
          f"{statistics.mean(b for _, b in cl):.3f}   "
          f"+{sum(d > 0 for d in cld)} / -{sum(d < 0 for d in cld)} / "
          f"={sum(d == 0 for d in cld)}   W+ = {cw:g}, p = {cp:.4f}")
    print(power_floor(cn, "clusters"))
    print(f"  run level  : {gained} gained, {lost} lost, "
          f"exact McNemar p = {mcnemar_p(gained, lost):.4f}")
    if primary:
        print()
        print("  Per cluster, the read the outcome rests on:")
        for (name, members), (a, b) in zip(
                [(k, v) for k, v in CLUSTERS.items()
                 if any(f in ra for f in v)], cl):
            print(f"    {name:16s} {a:6.3f} -> {b:6.3f}   {b - a:+7.3f}")


if control:
    contrast("control -> monooff (the Implies widening alone)",
             control, arm["monooff"])
contrast("monooff -> monoon (the monotone rewrite and the cloned assumption)",
         arm["monooff"], arm["monoon"], primary=True)
contrast("monoon -> monoship (elitism_rate 0.1 -> 0)",
         arm["monoon"], arm["monoship"])
if control:
    contrast("control -> monoship (the branch as it ships)",
             control, arm["monoship"])

# -- secondary (PLAN.md section 6) --------------------------------------------

print()
print("=" * 78)
print("SECONDARY -- per arm, reported and not gating")
print("=" * 78)
print(f"{'arm':10s} {'yield':>10s} {'timeouts':>9s} {'median wall':>12s} "
      f"{'total wall':>11s} {'median repairs':>15s}")
for a in ARMS:
    runs = list(arm[a].values())
    wall = [num(r["wall_time_s"]) for r in runs]
    reps = [num(r["n_repairs"]) for r in runs if truth(r["found_repair"])]
    print(f"{a:10s} {sum(truth(r['found_repair']) for r in runs):4d}/"
          f"{len(runs):<5d} {sum(truth(r['timed_out']) for r in runs):9d} "
          f"{statistics.median(wall):11.1f}s {sum(wall) / 3600:10.1f}h "
          f"{statistics.median(reps):15.1f}")

# -- against AuRUS (PLAN.md section 8, context and not a gate) -----------------
#
# The 2026-08-14 rule, with AuRUS's rate filtered to the repairs that are both
# realizable and well-separated -- counter's output gate rejects an
# ill-separated survivor unconditionally, so its own rate is over well-separated
# repairs by construction. The validation rows live in the control archive
# rather than here, this campaign having run no AuRUS arm of its own.

aurus = defaultdict(lambda: [0, 0])
for host in ("av2", "av3"):
    path = CONTROL / f"validation-{host}.csv"
    if not path.exists():
        aurus = None
        break
    for r in csv.DictReader(open(path)):
        aurus[r["spec"]][0] += truth(r["implies_genuine"])
        aurus[r["spec"]][1] += 1

if aurus:
    a_rate = {f: hit / n for f, (hit, n) in aurus.items()}
    print()
    print("=" * 78)
    print("AGAINST AuRUS -- context, cannot move the outcome")
    print("=" * 78)
    print(f"{'arm':10s} {'counter':>8s} {'AuRUS':>7s} {'p family':>10s} "
          f"{'p cluster':>10s}")
    named = [("control", control)] if control else []
    named += [(a, arm[a]) for a in ARMS]
    for label, runs in named:
        if not runs:
            continue
        r = rates(runs, sorted(runs))
        fams = sorted(set(r) & set(a_rate))
        p, _, _ = wilcoxon_p([r[f] - a_rate[f] for f in fams])
        cl = []
        for members in CLUSTERS.values():
            present = [f for f in members if f in r and f in a_rate]
            if present:
                cl.append((statistics.mean(r[f] for f in present),
                           statistics.mean(a_rate[f] for f in present)))
        cp, _, _ = wilcoxon_p([c - a for c, a in cl])
        print(f"{label:10s} {statistics.mean(r[f] for f in fams):8.3f} "
              f"{statistics.mean(a_rate[f] for f in fams):7.3f} "
              f"{p:10.4f} {cp:10.4f}")
else:
    print(f"\nnote: no AuRUS validation rows under {CONTROL}; "
          f"the AuRUS comparison is skipped", file=sys.stderr)

print()
print("=" * 78)
print("OUTCOME (PLAN.md section 5)")
print("=" * 78)
print("Section 5 registers three outcomes on the primary contrast and says the")
print("clustered read governs where the two disagree. Read the campaign's")
print("REPORT.md for which fired and what was decided.")
