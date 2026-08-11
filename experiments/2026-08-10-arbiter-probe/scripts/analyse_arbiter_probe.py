"""Pre-registered analysis for 2026-08-10-arbiter-probe (PLAN.md §5-§7).

Primary outcome is `found_repair`, paired by (spec, seed) between the two
selection schemes, McNemar read as an exact sign test on the discordant pairs
and Holm-corrected across the nine arbitration families. `lily02` is the
counter-signal control and is excluded from the correction, being the one
family the campaign does not expect the scheme to help.
"""
import collections
import csv
import math
import statistics
import sys

ARBITER_FAMILIES = [
    "arbiter", "arbiter-aurus", "arbiter-handshake", "full-arbiter",
    "prioritized-arbiter", "round-robin-arbiter", "simple-arbiter",
    "amba", "load-balancer",
]
CONTROL = "lily02"
TRUNC, APPOR = "nsga2-truncate", "nsga2-apportion"


def load(paths):
    rows = []
    for p in paths:
        with open(p) as f:
            rows.extend(csv.DictReader(f))
    return rows


def two_sided_binomial(k, n):
    """Exact two-sided sign-test p for k successes in n discordant pairs."""
    if n == 0:
        return 1.0
    tail = min(k, n - k)
    cum = sum(math.comb(n, i) for i in range(tail + 1)) / 2 ** n
    return min(1.0, 2 * cum)


def holm(pvals):
    order = sorted(range(len(pvals)), key=lambda i: pvals[i])
    out = [0.0] * len(pvals)
    running = 0.0
    for rank, i in enumerate(order):
        adj = min(1.0, (len(pvals) - rank) * pvals[i])
        running = max(running, adj)
        out[i] = running
    return out


rows = load(sys.argv[1:])
print(f"rows loaded: {len(rows)}")
commits = collections.Counter(r["commit"] for r in rows)
dirty = collections.Counter(r["dirty"] for r in rows)
print(f"commits: {dict(commits)}  dirty: {dict(dirty)}")
print(f"timed_out: {sum(1 for r in rows if r['timed_out'] == '1')}  "
      f"compare_timed_out: {sum(1 for r in rows if r['compare_timed_out'] == '1')}")

by = {(r["spec"], int(r["seed"]), r["selection"]): r for r in rows}
specs = ARBITER_FAMILIES + [CONTROL]

print("\n=== Primary: found_repair and wall time, paired by (spec, seed) ===")
print("wall columns are per-run medians in seconds; ratio is the median of the")
print("per-pair ratios, which is not the ratio of the two medians.")
print(f"{'family':<22}{'pairs':>6}{'trunc':>8}{'appor':>8}"
      f"{'A>T':>5}{'T>A':>5}{'p':>10}{'p_holm':>9}"
      f"{'wall_T':>9}{'wall_A':>9}{'ratio':>7}")

results, pvals, arb_idx = [], [], []
for spec in specs:
    pairs, wall_t, wall_a, ratios = [], [], [], []
    for seed in range(40):
        t, a = by.get((spec, seed, TRUNC)), by.get((spec, seed, APPOR))
        if t is None or a is None:
            continue
        pairs.append((int(t["found_repair"]), int(a["found_repair"])))
        wt, wa = float(t["wall_time_s"]), float(a["wall_time_s"])
        wall_t.append(wt)
        wall_a.append(wa)
        if wt > 0:
            ratios.append(wa / wt)
    if not pairs:
        continue
    n = len(pairs)
    t_rate = sum(t for t, _ in pairs) / n
    a_rate = sum(a for _, a in pairs) / n
    a_wins = sum(1 for t, a in pairs if a > t)
    t_wins = sum(1 for t, a in pairs if t > a)
    p = two_sided_binomial(a_wins, a_wins + t_wins)
    results.append((spec, n, t_rate, a_rate, a_wins, t_wins, p,
                    wall_t, wall_a, ratios))
    if spec in ARBITER_FAMILIES:
        arb_idx.append(len(results) - 1)
        pvals.append(p)

adj = holm(pvals)
adj_by_idx = dict(zip(arb_idx, adj))
for i, r in enumerate(results):
    spec, n, t_rate, a_rate, a_wins, t_wins, p, wall_t, wall_a, ratios = r
    tag = "" if spec in ARBITER_FAMILIES else "  <- control, not corrected"
    ph = f"{adj_by_idx[i]:.4f}" if i in adj_by_idx else "     -"
    print(f"{spec:<22}{n:>6}{t_rate:>8.3f}{a_rate:>8.3f}"
          f"{a_wins:>5}{t_wins:>5}{p:>10.4f}{ph:>9}"
          f"{statistics.median(wall_t):>9.1f}{statistics.median(wall_a):>9.1f}"
          f"{statistics.median(ratios):>7.2f}{tag}")

# Totals over every paired run, control included: this is what the campaign
# actually cost, not what the decision rule reads.
tot_t = sum(sum(r[7]) for r in results)
tot_a = sum(sum(r[8]) for r in results)
print(f"{'TOTAL (h)':<22}{sum(r[1] for r in results):>6}{'':>8}{'':>8}"
      f"{'':>5}{'':>5}{'':>10}{'':>9}"
      f"{tot_t / 3600:>9.2f}{tot_a / 3600:>9.2f}{tot_a / tot_t:>7.2f}")

sig_a = [results[i][0] for i in arb_idx
         if adj_by_idx[i] < 0.05 and results[i][3] > results[i][2]]
sig_t = [results[i][0] for i in arb_idx
         if adj_by_idx[i] < 0.05 and results[i][2] > results[i][3]]
print(f"\ncriterion 1 (>=3 families apportion-higher, Holm p<0.05): "
      f"{len(sig_a)} -> {sig_a}")
print(f"criterion 2 (no family truncate-higher at same threshold): "
      f"{len(sig_t)} -> {sig_t}")

print("\n=== Secondary: repair quality ===")
print("PLAN.md §7 makes quality a reported caveat rather than a gate, so this")
print("is stated in both directions wherever the two disagree.")
print(f"{'family':<22}{'ii_trunc':>10}{'ii_appor':>10}"
       f"{'nrep_T':>8}{'nrep_A':>8}")
ratios_all = []
for spec in specs:
    ii_t, ii_a, nr_t, nr_a = [], [], [], []
    for seed in range(40):
        t, a = by.get((spec, seed, TRUNC)), by.get((spec, seed, APPOR))
        if t is None or a is None:
            continue
        ii_t.append(float(t["implies_ideal"]))
        ii_a.append(float(a["implies_ideal"]))
        nr_t.append(float(t["n_repairs"]))
        nr_a.append(float(a["n_repairs"]))
        wt, wa = float(t["wall_time_s"]), float(a["wall_time_s"])
        if wt > 0:
            ratios_all.append(wa / wt)
    if not ii_t:
        continue
    print(f"{spec:<22}{statistics.mean(ii_t):>10.3f}{statistics.mean(ii_a):>10.3f}"
          f"{statistics.mean(nr_t):>8.1f}{statistics.mean(nr_a):>8.1f}")
print(f"\npooled median paired wall ratio (apportion / truncate): "
      f"{statistics.median(ratios_all):.2f}")
