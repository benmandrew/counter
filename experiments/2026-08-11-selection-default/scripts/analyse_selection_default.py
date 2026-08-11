"""Pre-registered analysis for 2026-08-11-selection-default (PLAN.md §6-§7).

Three arms, paired by (spec, seed) and identified by (selection, sweep):

    A  nsga2-truncate  / R   the incumbent default
    B  nsga2-apportion / R   the candidate
    C  nsga2-truncate  / S   the incumbent at matched compute

Two contrasts, B vs A and B vs C, computed independently. The two paths are
analysed separately and never pooled: implies_ideal is measured against a
different ideal set on each, so a pooled mean would average two different
quantities.

Usage:
    python3 analyse_selection_default.py results-seldefault.csv          # FRETISH
    python3 analyse_selection_default.py results-seldefault-tlsf.csv     # TLSF

Both per-host CSVs may be passed instead of the merged one; the result is
identical, which is how the merge is checked.
"""
import collections
import csv
import math
import random
import statistics
import sys

TRUNC, APPOR = "nsga2-truncate", "nsga2-apportion"
ARMS = {"A": (TRUNC, "R"), "B": (APPOR, "R"), "C": (TRUNC, "S")}

# PLAN.md §7. Fixed before launch; do not tune these to a result.
MARGIN = 0.05           # criterion 1: pooled implies_ideal CI lower bound
SPEC_REGRESSION = 0.15  # criterion 2: worst tolerated per-spec quality loss
WALL_RATIO_MAX = 2.0    # criterion 5: median paired wall ratio B/A
BOOTSTRAP = 10_000
SEED = 20260811         # fixed so the interval is reproducible, not resampled
                        # until it agrees with a preferred conclusion


def load(paths):
    rows = []
    for p in paths:
        with open(p) as f:
            rows.extend(csv.DictReader(f))
    return rows


def bootstrap_ci(diffs, level=0.95):
    """Percentile bootstrap CI for the mean of paired differences."""
    if not diffs:
        return float("nan"), float("nan"), float("nan")
    rng = random.Random(SEED)
    n = len(diffs)
    means = sorted(statistics.mean(rng.choices(diffs, k=n))
                   for _ in range(BOOTSTRAP))
    lo = means[int((1 - level) / 2 * BOOTSTRAP)]
    hi = means[int((1 + level) / 2 * BOOTSTRAP)]
    return statistics.mean(diffs), lo, hi


def mcnemar_exact(b, c):
    """Two-sided exact sign test on the discordant pairs of a matched 2x2."""
    n = b + c
    if n == 0:
        return 1.0
    tail = min(b, c)
    cum = sum(math.comb(n, i) for i in range(tail + 1)) / 2 ** n
    return min(1.0, 2 * cum)


def holm(pvals):
    order = sorted(range(len(pvals)), key=lambda i: pvals[i])
    out, running = [0.0] * len(pvals), 0.0
    for rank, i in enumerate(order):
        running = max(running, min(1.0, (len(pvals) - rank) * pvals[i]))
        out[i] = running
    return out


def mh_matched(strata):
    """Mantel-Haenszel pooled odds ratio over matched pairs stratified by spec.

    `strata` is a list of (b, c) discordant counts per spec. For matched pairs
    the MH estimator collapses to sum(b) / sum(c), and the stratified test is
    McNemar's on the summed discordants -- concordant pairs carry no
    information about the odds ratio and drop out of both.
    """
    B = sum(b for b, _ in strata)
    C = sum(c for _, c in strata)
    if B == 0 and C == 0:
        return float("nan"), float("nan"), float("nan"), 1.0, B, C
    if B == 0 or C == 0:
        chi = (B - C) ** 2 / (B + C)
        return (float("inf") if C == 0 else 0.0), float("nan"), float("nan"), \
            math.erfc(math.sqrt(chi / 2)), B, C
    odds = B / C
    se = math.sqrt(1 / B + 1 / C)
    lo, hi = math.exp(math.log(odds) - 1.96 * se), math.exp(math.log(odds) + 1.96 * se)
    chi = (B - C) ** 2 / (B + C)
    return odds, lo, hi, math.erfc(math.sqrt(chi / 2)), B, C


def wilcoxon(diffs):
    """Two-sided Wilcoxon signed-rank, normal approximation with tie correction."""
    nz = [d for d in diffs if d != 0]
    n = len(nz)
    if n < 10:
        return float("nan")
    order = sorted(range(n), key=lambda i: abs(nz[i]))
    ranks = [0.0] * n
    i = 0
    while i < n:
        j = i
        while j + 1 < n and abs(nz[order[j + 1]]) == abs(nz[order[i]]):
            j += 1
        avg = (i + j) / 2 + 1
        for k in range(i, j + 1):
            ranks[order[k]] = avg
        i = j + 1
    w = sum(r for d, r in zip(nz, ranks) if d > 0)
    mean = n * (n + 1) / 4
    sd = math.sqrt(n * (n + 1) * (2 * n + 1) / 24)
    if sd == 0:
        return float("nan")
    return math.erfc(abs(w - mean) / sd / math.sqrt(2))


rows = load(sys.argv[1:])
print(f"rows loaded: {len(rows)}")
print(f"commits: {dict(collections.Counter(r['commit'] for r in rows))}  "
      f"dirty: {dict(collections.Counter(r['dirty'] for r in rows))}")

by = {}
for r in rows:
    for arm, (scheme, sweep) in ARMS.items():
        if r["selection"] == scheme and r["sweep"] == sweep:
            by[(r["spec"], int(r["seed"]), arm)] = r
specs = sorted({r["spec"] for r in rows})
seeds = sorted({int(r["seed"]) for r in rows})

# A pair is usable only if BOTH arms of the contrast are present and neither
# had compare time out. The exclusion is symmetric by construction: dropping a
# single row rather than its pair is what biased the discarded df66e44
# execution of the arbiter probe, where the loss tracked what each arm explored.
def contrast(ref, spec_filter=None):
    kept, dropped_timeout, dropped_missing = [], 0, 0
    for spec in specs:
        if spec_filter and spec != spec_filter:
            continue
        for seed in seeds:
            x, y = by.get((spec, seed, ref)), by.get((spec, seed, "B"))
            if x is None or y is None:
                dropped_missing += 1
                continue
            if x["compare_timed_out"] == "1" or y["compare_timed_out"] == "1":
                dropped_timeout += 1
                continue
            kept.append((spec, x, y))
    return kept, dropped_timeout, dropped_missing


def report(ref):
    print(f"\n{'=' * 72}\n=== Contrast: B (apportion/R) vs {ref} "
          f"({ARMS[ref][0]}/{ARMS[ref][1]}) ===\n{'=' * 72}")
    kept, t_out, missing = contrast(ref)
    print(f"usable pairs: {len(kept)}   dropped (compare timeout): {t_out}   "
          f"dropped (arm missing): {missing}")
    if not kept:
        return None

    ii = [float(y["implies_ideal"]) - float(x["implies_ideal"])
          for _, x, y in kept]
    nr = [float(y["n_repairs"]) - float(x["n_repairs"]) for _, x, y in kept]
    ratios = [float(y["wall_time_s"]) / float(x["wall_time_s"])
              for _, x, y in kept if float(x["wall_time_s"]) > 0]

    m, lo, hi = bootstrap_ci(ii)
    print(f"\n[1] quality  implies_ideal mean paired diff {m:+.4f}  "
          f"95% CI [{lo:+.4f}, {hi:+.4f}]   margin -{MARGIN}")
    c1 = lo > -MARGIN
    print(f"    criterion 1 (CI lower bound > -{MARGIN}): {'PASS' if c1 else 'FAIL'}")

    print(f"\n[2] per-spec quality (regression bound {SPEC_REGRESSION})")
    worst, c2 = [], True
    for spec in specs:
        k, _, _ = contrast(ref, spec)
        if not k:
            continue
        d = [float(y["implies_ideal"]) - float(x["implies_ideal"]) for _, x, y in k]
        sm, slo, shi = bootstrap_ci(d)
        bad = sm < -SPEC_REGRESSION and shi < 0
        if bad:
            c2 = False
        worst.append((sm, spec, slo, shi, bad))
    for sm, spec, slo, shi, bad in sorted(worst)[:5]:
        print(f"    {spec:<22}{sm:+8.3f}  [{slo:+.3f}, {shi:+.3f}]"
              f"{'   <- BREACH' if bad else ''}")
    print(f"    criterion 2 (no spec below -{SPEC_REGRESSION} with CI excluding 0): "
          f"{'PASS' if c2 else 'FAIL'}")

    print(f"\n[3] yield  found_repair, Mantel-Haenszel stratified by spec")
    strata, per_spec = [], []
    for spec in specs:
        k, _, _ = contrast(ref, spec)
        b = sum(1 for _, x, y in k
                if int(y["found_repair"]) > int(x["found_repair"]))
        c = sum(1 for _, x, y in k
                if int(x["found_repair"]) > int(y["found_repair"]))
        strata.append((b, c))
        per_spec.append((spec, b, c, mcnemar_exact(b, c)))
    odds, olo, ohi, p, B, C = mh_matched(strata)
    print(f"    B>{ref}: {B}   {ref}>B: {C}   OR {odds:.3f}  "
          f"95% CI [{olo:.3f}, {ohi:.3f}]  p={p:.4g}")
    c3 = odds > 1 and olo > 1
    print(f"    criterion 3 (OR > 1, CI excluding 1): {'PASS' if c3 else 'FAIL'}")

    # Criterion 4 is "does not fall", so it reads the point estimate rather than
    # the interval: an interval straddling zero is what non-inferiority looks
    # like here, and requiring the lower bound above zero would silently
    # duplicate criterion 3's demand for a gain.
    m4, lo4, hi4 = bootstrap_ci(nr)
    c4 = m4 >= 0
    print(f"\n[4] n_repairs mean paired diff {m4:+.3f}  95% CI [{lo4:+.3f}, {hi4:+.3f}]")
    print(f"    criterion 4 (does not fall): {'PASS' if c4 else 'FAIL'}")

    print(f"\n[5] cost  median paired wall ratio B/{ref} "
          f"{statistics.median(ratios):.3f}   bound {WALL_RATIO_MAX}"
          f"   Wilcoxon p={wilcoxon([r - 1 for r in ratios]):.4g}")
    c5 = statistics.median(ratios) <= WALL_RATIO_MAX
    print(f"    criterion 5 (<= {WALL_RATIO_MAX}): {'PASS' if c5 else 'FAIL'}"
          + ("" if ref == "A" else "   [informational: criterion 5 is B vs A]"))

    print(f"\n    per-spec McNemar, Holm-corrected (reported, does not gate)")
    ps = [p for _, _, _, p in per_spec]
    for (spec, b, c, p), ph in zip(per_spec, holm(ps)):
        print(f"      {spec:<22} B>{ref}:{b:>3}  {ref}>B:{c:>3}  "
              f"p={p:.4f}  p_holm={ph:.4f}")
    return {"c1": c1, "c2": c2, "c3": c3, "c4": c4, "c5": c5}


res_a = report("A")
res_c = report("C")

print(f"\n{'=' * 72}\n=== Decision (PLAN.md §7) ===\n{'=' * 72}")
if res_a and res_c:
    checks = [
        ("1  quality non-inferior vs A and C", res_a["c1"] and res_c["c1"]),
        ("2  no spec regression vs A or C", res_a["c2"] and res_c["c2"]),
        ("3  yield improves vs A and C", res_a["c3"] and res_c["c3"]),
        ("4  n_repairs does not fall", res_a["c4"] and res_c["c4"]),
        ("5  median wall ratio B/A <= 2.0", res_a["c5"]),
    ]
    for label, ok in checks:
        print(f"  {'PASS' if ok else 'FAIL'}  {label}")
    if all(ok for _, ok in checks):
        print("\n  -> all five hold on this path: nsga2-apportion becomes the default")
    elif res_a["c3"] and not res_c["c3"]:
        print("\n  -> beats the incumbent but not the compute control: the gain is "
              "compute, not scheme")
    else:
        print("\n  -> stays opt-in")
    print("\n  Both paths must clear independently; run this script on the other "
          "CSV before concluding.")
