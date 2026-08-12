#!/usr/bin/env python3
"""Pre-registered analysis for the 2026-08-11 status-grading campaign.

Endpoints, in the order PLAN.md registers them:
  1. paired yield        -- found_repair, per (spec, seed)
  2. paired implies_ideal
  3. paired wall time

Pairing is on (spec, seed), which is what makes a host difference cancel: both
arms of a pair ran on the same host. The sign test is over the discordant pairs
alone, so the tied pairs reach the reported rates and not the test.

Written after the campaign to read the merged CSV, so this is not a vendored
driver copy -- it ran nothing. Usage:

    python3 analyse_status_grading.py [path/to/results-status-grading.csv]
"""
import csv
import math
import statistics
import sys
from collections import defaultdict

PATH = (sys.argv[1] if len(sys.argv) > 1
        else "experiments/results-status-grading.csv")

rows = list(csv.DictReader(open(PATH)))
print(f"rows: {len(rows)}")

# --- integrity -------------------------------------------------------------
arms = sorted({r["level_name"] for r in rows})
commits = sorted({r["commit"] for r in rows})
dirty = sorted({r["dirty"] for r in rows})
print(f"arms: {arms}   commits: {commits}   dirty: {dirty}")

by_arm = defaultdict(list)
for r in rows:
    by_arm[r["level_name"]].append(r)
for a in arms:
    print(f"  {a}: {len(by_arm[a])} rows")

# --- pair up ---------------------------------------------------------------
pairs = defaultdict(dict)
for r in rows:
    pairs[(r["spec"], int(r["seed"]))][r["level_name"]] = r
complete = {k: v for k, v in pairs.items() if len(v) == 2}
print(f"pairs: {len(pairs)} total, {len(complete)} complete\n")


def truth(s):
    return str(s).strip().lower() in ("1", "true", "yes")


def num(s, default=float("nan")):
    try:
        return float(s)
    except (TypeError, ValueError):
        return default


def sign_test(wins, losses):
    """Two-sided exact binomial p at q=0.5 over the discordant pairs."""
    n = wins + losses
    if n == 0:
        return 1.0
    k = min(wins, losses)
    tail = sum(math.comb(n, i) for i in range(0, k + 1)) / (2 ** n)
    return min(1.0, 2 * tail)


# --- endpoint 1: yield -----------------------------------------------------
print("=" * 74)
print("ENDPOINT 1 -- paired yield (found_repair)")
print("=" * 74)
y = {a: sum(truth(r["found_repair"]) for r in by_arm[a]) for a in arms}
for a in arms:
    print(f"  {a:8s} {y[a]:3d}/{len(by_arm[a]):3d}  ({100*y[a]/len(by_arm[a]):.1f}%)")

mrs_win = tie = tiered_win = 0
yield_by_spec = defaultdict(lambda: {"tiered": 0, "mrs": 0, "n": 0})
for (spec, seed), p in complete.items():
    t, m = truth(p["tiered"]["found_repair"]), truth(p["mrs"]["found_repair"])
    yield_by_spec[spec]["tiered"] += t
    yield_by_spec[spec]["mrs"] += m
    yield_by_spec[spec]["n"] += 1
    if m and not t:
        mrs_win += 1
    elif t and not m:
        tiered_win += 1
    else:
        tie += 1
print(f"\n  paired: mrs-only {mrs_win}, tiered-only {tiered_win}, tied {tie}")
print(f"  sign test p = {sign_test(mrs_win, tiered_win):.4f}")

print("\n  per spec (tiered -> mrs, out of n):")
for spec in sorted(yield_by_spec):
    d = yield_by_spec[spec]
    mark = ""
    if d["mrs"] > d["tiered"]:
        mark = "  <- mrs"
    elif d["tiered"] > d["mrs"]:
        mark = "  <- tiered"
    print(f"    {spec:22s} {d['tiered']:2d} -> {d['mrs']:2d}   /{d['n']:2d}{mark}")

# --- endpoint 2: implies_ideal ---------------------------------------------
print()
print("=" * 74)
print("ENDPOINT 2 -- paired implies_ideal (repair quality)")
print("=" * 74)
for a in arms:
    vals = [num(r["implies_ideal"]) for r in by_arm[a]
            if truth(r["found_repair"]) and not math.isnan(num(r["implies_ideal"]))]
    if vals:
        print(f"  {a:8s} n={len(vals):3d}  mean {statistics.mean(vals):.4f}  "
              f"median {statistics.median(vals):.4f}  max {max(vals):.4f}")
    else:
        print(f"  {a:8s} no scored rows")

qw = ql = qt = 0
deltas = []
for (spec, seed), p in complete.items():
    tv, mv = num(p["tiered"]["implies_ideal"]), num(p["mrs"]["implies_ideal"])
    if math.isnan(tv) or math.isnan(mv):
        continue
    deltas.append(mv - tv)
    if mv > tv:
        qw += 1
    elif tv > mv:
        ql += 1
    else:
        qt += 1
print(f"\n  pairs with both scored: {qw+ql+qt}")
print(f"  mrs better {qw}, tiered better {ql}, equal {qt}")
print(f"  sign test p = {sign_test(qw, ql):.4f}")
if deltas:
    print(f"  mean delta (mrs - tiered) = {statistics.mean(deltas):+.4f}")

# The endpoint-2 test above runs over every pair, so a pair where only one arm
# yielded enters it as a quality difference. That conflates "mrs found a better
# repair" with "mrs found a repair at all", which endpoint 1 already measures.
# Restricting to the pairs where both arms yielded separates the two, and is the
# comparison that decides whether the extra yield costs quality.
bw = bl = be = 0
both = []
for (spec, seed), p in complete.items():
    if not (truth(p["tiered"]["found_repair"])
            and truth(p["mrs"]["found_repair"])):
        continue
    tv, mv = num(p["tiered"]["implies_ideal"]), num(p["mrs"]["implies_ideal"])
    if math.isnan(tv) or math.isnan(mv):
        continue
    both.append(mv - tv)
    if mv > tv:
        bw += 1
    elif tv > mv:
        bl += 1
    else:
        be += 1
print(f"\n  restricted to the {len(both)} pairs where BOTH arms yielded:")
print(f"    mrs better {bw}, tiered better {bl}, equal {be}")
if both:
    print(f"    mean delta {statistics.mean(both):+.4f}")

# And the mirror question: what do mrs's extra repairs actually score? A high
# yield of implies_ideal = 0 repairs is still a real result -- they are
# realizable weakenings -- but it is not the same claim as matching the ideal.
extra = defaultdict(list)
for (spec, seed), p in complete.items():
    if truth(p["mrs"]["found_repair"]) and not truth(p["tiered"]["found_repair"]):
        extra[spec].append(num(p["mrs"]["implies_ideal"]))
if extra:
    print("\n  quality of the mrs-only wins:")
    for spec in sorted(extra):
        v = [x for x in extra[spec] if not math.isnan(x)]
        if not v:
            continue
        print(f"    {spec:22s} n={len(v):2d}  mean {statistics.mean(v):.3f}  "
              f"max {max(v):.3f}  nonzero {sum(1 for x in v if x > 0)}")

# --- endpoint 3: wall time -------------------------------------------------
print()
print("=" * 74)
print("ENDPOINT 3 -- paired wall time")
print("=" * 74)
for a in arms:
    v = [num(r["wall_time_s"]) for r in by_arm[a]]
    v = [x for x in v if not math.isnan(x)]
    print(f"  {a:8s} total {sum(v)/3600:6.2f} h   mean {statistics.mean(v):7.1f}s   "
          f"median {statistics.median(v):7.1f}s")

ratios = []
per_spec_ratio = defaultdict(list)
for (spec, seed), p in complete.items():
    tv, mv = num(p["tiered"]["wall_time_s"]), num(p["mrs"]["wall_time_s"])
    if math.isnan(tv) or math.isnan(mv) or tv <= 0:
        continue
    ratios.append(mv / tv)
    per_spec_ratio[spec].append(mv / tv)
if ratios:
    ratios.sort()
    print(f"\n  paired mrs/tiered wall ratio over {len(ratios)} pairs:")
    print(f"    median {statistics.median(ratios):.2f}x   "
          f"mean {statistics.mean(ratios):.2f}x   "
          f"min {ratios[0]:.2f}x   max {ratios[-1]:.2f}x")
    print("\n  median ratio per spec:")
    for spec in sorted(per_spec_ratio, key=lambda s: -statistics.median(per_spec_ratio[s])):
        rs = per_spec_ratio[spec]
        print(f"    {spec:22s} {statistics.median(rs):5.2f}x  (n={len(rs)})")

# --- censoring -------------------------------------------------------------
print()
print("=" * 74)
print("CENSORING -- timed_out")
print("=" * 74)
for a in arms:
    to = [r for r in by_arm[a] if truth(r["timed_out"])]
    print(f"  {a:8s} {len(to):3d}/{len(by_arm[a])} timed out")
    bys = defaultdict(int)
    for r in to:
        bys[r["spec"]] += 1
    for s in sorted(bys, key=lambda s: -bys[s]):
        print(f"      {s:22s} {bys[s]}")
ct = sum(truth(r["compare_timed_out"]) for r in rows)
print(f"  compare_timed_out (either arm): {ct}")
