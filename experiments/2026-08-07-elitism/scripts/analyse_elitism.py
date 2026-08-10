#!/usr/bin/env python3
"""Pre-registered analysis for the elitism-default campaign (PLAN.md section 7).

Pairs rows by (spec, seed) within path and reports the four decision criteria.
The CI on a paired binary difference is McNemar's, on the discordant counts:
a pooled two-sample interval would ignore the pairing and be too wide.
"""
import argparse
import collections
import csv
import math
import pathlib

Z = 1.959963984540054  # two-sided 95%


def load(path, label):
    rows = []
    with open(path) as fh:
        for r in csv.DictReader(fh):
            if r["sweep"] != "R":
                continue
            r["_path"] = label
            rows.append(r)
    return rows


def pair(rows, field, cast=float):
    """Return {(spec, seed): (value_at_elit0, value_at_elit0.1)} for complete pairs."""
    by = {}
    for r in rows:
        try:
            v = cast(r[field])
        except (ValueError, KeyError):
            v = None
        by.setdefault((r["spec"], r["seed"]), {})[r["level_name"]] = v
    out = {}
    for k, arms in by.items():
        a, b = arms.get("elit0"), arms.get("elit0.1")
        if a is not None and b is not None:
            out[k] = (a, b)
    return out


def mcnemar(pairs):
    """Difference (elit0.1 - elit0) with a 95% CI, on paired binary outcomes."""
    n = len(pairs)
    b = sum(1 for a, c in pairs.values() if c > a)   # 0.1 wins
    c = sum(1 for a, d in pairs.values() if a > d)   # 0 wins
    diff = (b - c) / n
    var = (b + c - (b - c) ** 2 / n) / n ** 2 if n else 0.0
    se = math.sqrt(max(var, 0.0))
    return n, b, c, diff, diff - Z * se, diff + Z * se


def report(label, rows):
    print(f"\n{'=' * 74}\n  {label}  ({len(rows)} rows)\n{'=' * 74}")
    specs = sorted({r["spec"] for r in rows})
    print(f"  specs: {len(specs)}   seeds: {len(set(r['seed'] for r in rows))}")

    censored = sum(1 for r in rows if r.get("timed_out") == "1")
    ctimeout = sum(1 for r in rows if r.get("compare_timed_out") == "1")
    print(f"  timed_out rows: {censored}    compare_timed_out rows: {ctimeout}")

    verdicts = {}
    for field in ("found_repair", "implies_ideal"):
        pairs = pair(rows, field, lambda s: int(float(s)))
        n, b, c, diff, lo, hi = mcnemar(pairs)
        m0 = sum(a for a, _ in pairs.values()) / n
        m1 = sum(d for _, d in pairs.values()) / n
        ok = hi < 0.05
        verdicts[field] = ok
        print(f"\n  {field}")
        print(f"    pairs={n}  elit0={m0:.4f}  elit0.1={m1:.4f}")
        print(f"    discordant: 0.1-only={b}  0-only={c}")
        print(f"    diff (0.1 - 0) = {diff:+.4f}   95% CI [{lo:+.4f}, {hi:+.4f}]")
        print(f"    non-inferiority (upper < +0.05): {'PASS' if ok else 'FAIL'}")

    pairs = pair(rows, "wall_time_s")
    t0 = sum(a for a, _ in pairs.values())
    t1 = sum(b for _, b in pairs.values())
    ratio = t0 / t1 if t1 else float("nan")
    ok = ratio <= 1.10
    verdicts["wall_time_s"] = ok
    print(f"\n  wall_time_s")
    print(f"    pairs={len(pairs)}  elit0 mean={t0/len(pairs):.2f}s  "
          f"elit0.1 mean={t1/len(pairs):.2f}s")
    print(f"    cost ratio (0 / 0.1) = {ratio:.3f}")
    print(f"    criterion 4 (0 no more than 10% above 0.1): "
          f"{'PASS' if ok else 'FAIL'}")

    print("\n  best_relation distribution")
    dist = collections.defaultdict(collections.Counter)
    for r in rows:
        dist[r["level_name"]][r.get("best_relation", "")] += 1
    keys = sorted({k for c in dist.values() for k in c})
    print(f"    {'relation':<20}{'elit0':>10}{'elit0.1':>10}")
    for k in keys:
        print(f"    {k or '(blank)':<20}{dist['elit0'][k]:>10}{dist['elit0.1'][k]:>10}")

    print("\n  per-spec implies_ideal (elit0 -> elit0.1)")
    for s in specs:
        sub = [r for r in rows if r["spec"] == s]
        p = pair(sub, "implies_ideal", lambda x: int(float(x)))
        if not p:
            continue
        a = sum(v for v, _ in p.values()) / len(p)
        b = sum(v for _, v in p.values()) / len(p)
        flag = "  <-- differs" if abs(a - b) > 1e-9 else ""
        print(f"    {s:<24} n={len(p):>3}  {a:.3f} -> {b:.3f}{flag}")

    return verdicts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fret", default="experiments/results-elitism.csv")
    ap.add_argument("--tlsf", default="experiments/results-elitism-tlsf.csv")
    args = ap.parse_args()

    allv = {}
    for label, path in (("FRETISH", args.fret), ("TLSF", args.tlsf)):
        p = pathlib.Path(path)
        if not p.exists():
            print(f"missing: {p}")
            continue
        allv[label] = report(label, load(p, label))

    print(f"\n{'=' * 74}\n  DECISION (PLAN.md section 7)\n{'=' * 74}")
    crit = {
        1: ("implies_ideal non-inferior, both paths", "implies_ideal"),
        2: ("found_repair non-inferior, both paths", "found_repair"),
        4: ("elit0 not >10% costlier, both paths", "wall_time_s"),
    }
    switch = True
    for num, (desc, field) in sorted(crit.items()):
        ok = all(v.get(field, False) for v in allv.values())
        switch &= ok
        print(f"  {num}. {desc:<44} {'PASS' if ok else 'FAIL'}")
    print("  3. zero trivial repairs in elit0            (separate audit)")
    print(f"\n  -> on criteria 1/2/4: "
          f"{'switch default to 0.0' if switch else 'KEEP elitism_rate = 0.1'}")


if __name__ == "__main__":
    main()
