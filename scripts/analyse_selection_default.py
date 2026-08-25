#!/usr/bin/env python3
"""Scores the 2026-08-11-selection-default campaign against its own PLAN.md.

This is a re-implementation, not the pre-registration. The binding analysis is
the copy committed with the plan before launch, at

    experiments/2026-08-11-selection-default/scripts/analyse_selection_default.py

and where the two ever disagree that one is right by construction: it was
fixed before a row existed and this one was not. It is kept beside it as an
independent reading of the same prose, written from PLAN.md rather than from
the pre-registered code, so that the judgement calls section 6 leaves open are
made twice and can be seen to agree. On the campaign's own data they agree on
every one of the five criteria and on both paths.

The question is whether `nsga2-apportion` should replace `nsga2-truncate` as
the `Config::selection_scheme` default. Three arms, fully paired by
`(spec, seed)`:

    A  nsga2-truncate   at the shipped generation count   the incumbent
    B  nsga2-apportion  at the shipped generation count   the candidate
    C  nsga2-truncate   at a scaled generation count      the compute control

Arm C is what separates "apportion searches better" from "apportion searches
longer", and it is the arm both prior campaigns lacked. Two contrasts are
computed independently and reported side by side, B vs A and B vs C. The two
paths are never pooled: `implies_ideal` is scored against a different ideal set
on each, so a pooled mean over both would not be a mean of anything.

The five criteria of PLAN.md section 7 are evaluated by this script rather than
by eye, and it prints the decision. Two judgement calls the prose leaves open
are settled here and named in the output where they bite:

  * A pair is dropped whole when either side has `compare_timed_out = 1`, and
    likewise when either side's run is missing. An unpaired drop is what
    biased the discarded execution of the arbiter probe.
  * Criterion 4 ("`n_repairs` does not fall") is read on the point estimate,
    since section 6 specifies only a mean paired difference for `n_repairs`
    and specifies a bootstrap interval for quality where it wants one. The
    interval is printed beside it so the stricter reading is checkable.

The bootstrap seed is fixed. An interval that can be resampled is an interval
that can be resampled until it agrees.

Usage:
    python3 scripts/analyse_selection_default.py \
        --fretish experiments/results-seldefault.csv \
        --tlsf experiments/results-seldefault-tlsf.csv
    python3 scripts/analyse_selection_default.py --self-test
"""

import argparse
import math
import random
import statistics
from pathlib import Path

from analysis_lib import (load_rows, mcnemar_exact, num_or_zero, pair_up,
                          signed_ranks)

# PLAN.md section 7.
QUALITY_MARGIN = -0.05          # criterion 1, pooled CI lower bound must exceed
SPEC_DAMAGE = -0.15             # criterion 2, per-spec mean floor
WALL_RATIO_BOUND = 2.0          # criterion 5, median paired B/A

BOOTSTRAP_RESAMPLES = 10000
BOOTSTRAP_SEED = 20260811       # the pre-registration date, fixed once
Z95 = 1.959963984540054

ARM_LABEL = {
    "A": "nsga2-truncate (shipped generations)",
    "B": "nsga2-apportion (shipped generations)",
    "C": "nsga2-truncate (compute-matched)",
}


def parse_args() -> argparse.Namespace:
    ap = argparse.ArgumentParser(
        description="score the selection-default campaign against its plan")
    ap.add_argument("--fretish", type=Path, help="the FRETISH results CSV")
    ap.add_argument("--tlsf", type=Path, help="the TLSF results CSV")
    ap.add_argument("--self-test", action="store_true",
                    help="run against a synthetic dataset with a planted "
                         "effect and check the verdict is recovered")
    return ap.parse_args()


# ---------------------------------------------------------------- loading


def arm_of(row) -> str:
    """A, B or C from the two columns that distinguish them.

    The compute-matched phase carries its own `level_name`; within the base
    phase the arm is the selection scheme.
    """
    if row["level_name"].startswith("cm-"):
        return "C"
    return "B" if row["selection"] == "nsga2-apportion" else "A"


def load(path) -> list:
    rows = load_rows(path)
    for row in rows:
        row["arm"] = arm_of(row)
    return rows


def paired(pairs, left, right, column):
    """Paired differences, candidate minus comparator, in file order.

    A blank or unparseable cell reads as 0.0, not nan: every column paired here
    is a count or a score whose absence on a completed run means "none", and a
    nan would take the arm's whole mean with it.
    """
    return [num_or_zero(p[left][column]) - num_or_zero(p[right][column])
            for _, p in pairs]


# ---------------------------------------------------------------- statistics


def bootstrap_ci(values, seed=BOOTSTRAP_SEED, resamples=BOOTSTRAP_RESAMPLES):
    """Percentile bootstrap 95% CI on the mean. Deterministic in `seed`."""
    if not values:
        return (float("nan"), float("nan"))
    rng = random.Random(seed)
    n = len(values)
    means = []
    for _ in range(resamples):
        total = 0.0
        for _ in range(n):
            total += values[rng.randrange(n)]
        means.append(total / n)
    means.sort()
    lo = means[int(0.025 * resamples)]
    hi = means[min(resamples - 1, int(0.975 * resamples))]
    return (lo, hi)


def holm(pvalues) -> list:
    """Holm step-down adjusted p-values, in the input order."""
    indexed = sorted(range(len(pvalues)), key=lambda i: pvalues[i])
    adjusted = [0.0] * len(pvalues)
    running = 0.0
    for rank, i in enumerate(indexed):
        value = min(1.0, (len(pvalues) - rank) * pvalues[i])
        running = max(running, value)
        adjusted[i] = running
    return adjusted


def normal_sf(z) -> float:
    return 0.5 * math.erfc(z / math.sqrt(2.0))


def cmh_odds_ratio(strata):
    """Mantel-Haenszel pooled odds ratio over matched pairs, by spec.

    `strata` is a list of (b, c) discordant counts per spec: pairs the
    candidate won and pairs the comparator won. The design is paired, so the
    matched-pairs MH estimator is the right one and it collapses to
    sum(b) / sum(c) -- concordant pairs carry no information about the odds
    ratio and drop out. Using the unmatched stratified 2x2 form here would
    throw the pairing away and is not what the plan pre-registered.
    """
    b_sum = sum(b for b, _ in strata)
    c_sum = sum(c for _, c in strata)
    if b_sum == 0 and c_sum == 0:
        return (float("nan"), float("nan"), float("nan"))
    if b_sum == 0 or c_sum == 0:
        return (0.0 if b_sum == 0 else float("inf"),
                float("nan"), float("nan"))
    odds = b_sum / c_sum
    half = Z95 * math.sqrt(1.0 / b_sum + 1.0 / c_sum)
    return (odds, odds * math.exp(-half), odds * math.exp(half))


def wilcoxon_signed_rank(differences) -> float:
    """Two-sided Wilcoxon signed-rank, normal approximation.

    Zero differences are dropped (the classic treatment, not Pratt's), ties
    share a mid-rank and the variance carries the tie correction. n here is in
    the hundreds, so the approximation is not the loose part of the analysis.
    """
    w_plus, ranks, tie_term = signed_ranks(differences)
    n = len(ranks)
    if n == 0:
        return 1.0
    mean = n * (n + 1) / 4.0
    var = n * (n + 1) * (2 * n + 1) / 24.0 - tie_term / 48.0
    if var <= 0.0:
        return 1.0
    z = (abs(w_plus - mean) - 0.5) / math.sqrt(var)
    return min(1.0, 2.0 * normal_sf(z))


# ---------------------------------------------------------------- contrasts


class Contrast:
    """One paired comparison, candidate against comparator, on one path."""

    def __init__(self, rows, candidate, comparator, seed_offset):
        self.candidate = candidate
        self.comparator = comparator
        self.pairs, self.n_missing, self.n_timed_out = pair_up(
            rows, candidate, comparator)
        self.specs = sorted({k[0] for k, _ in self.pairs})

        quality = paired(self.pairs, candidate, comparator, "implies_ideal")
        self.quality_mean = statistics.fmean(quality) if quality else 0.0
        self.quality_ci = bootstrap_ci(quality, BOOTSTRAP_SEED + seed_offset)

        repairs = paired(self.pairs, candidate, comparator, "n_repairs")
        self.repairs_mean = statistics.fmean(repairs) if repairs else 0.0
        self.repairs_ci = bootstrap_ci(repairs, BOOTSTRAP_SEED + seed_offset)

        self.per_spec = {}
        for spec in self.specs:
            subset = [p for k, p in self.pairs if k[0] == spec]
            diffs = [num_or_zero(p[candidate]["implies_ideal"])
                     - num_or_zero(p[comparator]["implies_ideal"]) for p in subset]
            mean = statistics.fmean(diffs) if diffs else 0.0
            lo, hi = bootstrap_ci(diffs, BOOTSTRAP_SEED + seed_offset)
            b = sum(1 for p in subset
                    if p[candidate]["found_repair"] == "1"
                    and p[comparator]["found_repair"] != "1")
            c = sum(1 for p in subset
                    if p[candidate]["found_repair"] != "1"
                    and p[comparator]["found_repair"] == "1")
            self.per_spec[spec] = {
                "n": len(subset), "mean": mean, "lo": lo, "hi": hi,
                "b": b, "c": c, "p": mcnemar_exact(b, c),
                "yield_cand": statistics.fmean(
                    [float(p[candidate]["found_repair"] == "1")
                     for p in subset]) if subset else 0.0,
                "yield_comp": statistics.fmean(
                    [float(p[comparator]["found_repair"] == "1")
                     for p in subset]) if subset else 0.0,
            }
        adjusted = holm([self.per_spec[s]["p"] for s in self.specs])
        for spec, value in zip(self.specs, adjusted):
            self.per_spec[spec]["p_holm"] = value

        strata = [(self.per_spec[s]["b"], self.per_spec[s]["c"])
                  for s in self.specs]
        self.discordant = (sum(b for b, _ in strata),
                           sum(c for _, c in strata))
        self.odds, self.odds_lo, self.odds_hi = cmh_odds_ratio(strata)

        ratios = [num_or_zero(p[candidate]["wall_time_s"])
                  / num_or_zero(p[comparator]["wall_time_s"])
                  for _, p in self.pairs
                  if num_or_zero(p[comparator]["wall_time_s"]) > 0]
        self.wall_ratios = ratios
        self.wall_median = statistics.median(ratios) if ratios else float("nan")
        self.wall_p = wilcoxon_signed_rank([math.log(r) for r in ratios if r > 0])

        self.yield_cand = statistics.fmean(
            [float(p[candidate]["found_repair"] == "1")
             for _, p in self.pairs]) if self.pairs else 0.0
        self.yield_comp = statistics.fmean(
            [float(p[comparator]["found_repair"] == "1")
             for _, p in self.pairs]) if self.pairs else 0.0

    @property
    def name(self) -> str:
        return f"{self.candidate} vs {self.comparator}"

    def damaged_specs(self) -> list:
        return [s for s in self.specs
                if self.per_spec[s]["mean"] < SPEC_DAMAGE
                and self.per_spec[s]["hi"] < 0.0]


# ---------------------------------------------------------------- reporting


def report_arms(rows, label) -> None:
    print(f"\n{'=' * 74}\n{label}\n{'=' * 74}")
    print(f"{len(rows)} rows\n")
    print(f"  {'arm':3s} {'n':>5s} {'found_repair':>13s} "
          f"{'implies_ideal':>14s} {'n_repairs':>10s} {'wall_s':>9s}   scheme")
    for arm in ("A", "B", "C"):
        subset = [r for r in rows if r["arm"] == arm]
        if not subset:
            continue
        print(f"  {arm:3s} {len(subset):5d} "
              f"{statistics.fmean([float(r['found_repair'] == '1') for r in subset]):13.3f} "
              f"{statistics.fmean([num_or_zero(r['implies_ideal']) for r in subset]):14.4f} "
              f"{statistics.fmean([num_or_zero(r['n_repairs']) for r in subset]):10.3f} "
              f"{statistics.fmean([num_or_zero(r['wall_time_s']) for r in subset]):9.1f}"
              f"   {ARM_LABEL[arm]}")


def report_contrast(contrast) -> None:
    print(f"\n-- {contrast.name}  ({len(contrast.pairs)} pairs; "
          f"{contrast.n_missing} incomplete, "
          f"{contrast.n_timed_out} dropped for a timed-out comparison)")

    lo, hi = contrast.quality_ci
    print(f"   quality   mean paired implies_ideal {contrast.quality_mean:+.4f}  "
          f"95% CI [{lo:+.4f}, {hi:+.4f}]   margin {QUALITY_MARGIN:+.2f}")
    b, c = contrast.discordant
    print(f"   yield     {contrast.candidate} {contrast.yield_cand:.3f}  "
          f"{contrast.comparator} {contrast.yield_comp:.3f}   "
          f"discordant {b}/{c}   CMH OR {contrast.odds:.3f}  95% CI "
          f"[{contrast.odds_lo:.3f}, {contrast.odds_hi:.3f}]")
    rlo, rhi = contrast.repairs_ci
    print(f"   n_repairs mean paired {contrast.repairs_mean:+.4f}  "
          f"95% CI [{rlo:+.4f}, {rhi:+.4f}]")
    print(f"   cost      median paired wall ratio {contrast.wall_median:.3f}  "
          f"Wilcoxon p {contrast.wall_p:.3g}   bound {WALL_RATIO_BOUND:.1f}")

    print(f"\n   per spec (McNemar exact on found_repair, Holm-corrected; "
          f"reported, does not gate)")
    print(f"      {'spec':22s} {'n':>4s} {'d implies':>10s} "
          f"{'95% CI':>19s} {'yield':>13s} {'b/c':>9s} {'p_holm':>9s}")
    for spec in contrast.specs:
        s = contrast.per_spec[spec]
        flag = " DAMAGED" if spec in contrast.damaged_specs() else ""
        print(f"      {spec:22s} {s['n']:4d} {s['mean']:+10.4f} "
              f"[{s['lo']:+7.4f},{s['hi']:+7.4f}] "
              f"{s['yield_cand']:5.2f}/{s['yield_comp']:5.2f} "
              f"{s['b']:4d}/{s['c']:4d} {s['p_holm']:9.4f}{flag}")


def evaluate(path_label, vs_a, vs_c) -> bool:
    """The five criteria of PLAN.md section 7, for one path."""
    print(f"\n-- decision rule, {path_label}")
    verdicts = []

    for contrast in (vs_a, vs_c):
        ok = contrast.quality_ci[0] > QUALITY_MARGIN
        verdicts.append(("1 quality non-inferior vs "
                         f"{contrast.comparator}", ok,
                         f"CI lower {contrast.quality_ci[0]:+.4f} "
                         f"{'>' if ok else 'not >'} {QUALITY_MARGIN:+.2f}"))

    damaged = sorted(set(vs_a.damaged_specs()) | set(vs_c.damaged_specs()))
    verdicts.append(("2 no spec badly damaged", not damaged,
                     "none" if not damaged else ", ".join(damaged)))

    for contrast in (vs_a, vs_c):
        ok = contrast.odds_lo > 1.0
        verdicts.append((f"3 yield improves vs {contrast.comparator}", ok,
                         f"CMH OR {contrast.odds:.3f} CI "
                         f"[{contrast.odds_lo:.3f}, {contrast.odds_hi:.3f}]"))

    for contrast in (vs_a, vs_c):
        ok = contrast.repairs_mean >= 0.0
        verdicts.append((f"4 n_repairs does not fall vs {contrast.comparator}",
                         ok,
                         f"mean {contrast.repairs_mean:+.4f} (point estimate; "
                         f"CI [{contrast.repairs_ci[0]:+.4f}, "
                         f"{contrast.repairs_ci[1]:+.4f}])"))

    ok = vs_a.wall_median <= WALL_RATIO_BOUND
    verdicts.append(("5 cost bounded (B/A)", ok,
                     f"median ratio {vs_a.wall_median:.3f} "
                     f"{'<=' if ok else '>'} {WALL_RATIO_BOUND:.1f}"))

    for name, ok, detail in verdicts:
        print(f"      [{'PASS' if ok else 'FAIL':4s}] {name:44s} {detail}")
    passed = all(ok for _, ok, _ in verdicts)
    print(f"   {path_label}: "
          f"{'all criteria hold' if passed else 'criteria FAIL'}")
    return passed


def deliverable(fretish_pass, tlsf_pass, contrasts) -> None:
    """PLAN.md section 7's three named outcomes, in its own order."""
    print(f"\n{'=' * 74}\nVERDICT\n{'=' * 74}")
    if fretish_pass and tlsf_pass:
        print("All five criteria hold on both paths. The default changes in\n"
              "include/config.hpp, example-config.toml and "
              "docs/configuration.rst.")
        return
    # Criterion 3 against C alone is the plan's second named outcome.
    only_c = True
    for label, (vs_a, vs_c) in contrasts.items():
        a_ok = (vs_a.quality_ci[0] > QUALITY_MARGIN and vs_a.odds_lo > 1.0
                and vs_a.repairs_mean >= 0.0)
        c_fails_yield = not vs_c.odds_lo > 1.0
        if not (a_ok and c_fails_yield):
            only_c = False
        print(f"   {label}: beats A on yield "
              f"{'yes' if vs_a.odds_lo > 1.0 else 'no'}; "
              f"beats C on yield "
              f"{'yes' if vs_c.odds_lo > 1.0 else 'no'}")
    if only_c:
        print("\nCriterion 3 fails against C alone: the gain is compute, not\n"
              "scheme. docs/configuration.rst records that, and the "
              "generations\ndefault is opened as a separate question.")
    else:
        print("\nThe scheme stays opt-in. docs/configuration.rst gains the "
              "narrow\nguidance the arbiter probe earned, with the breadth "
              "result stated so\nthe recommendation is not read as general.")


def analyse(rows, label):
    report_arms(rows, label)
    vs_a = Contrast(rows, "B", "A", 0)
    vs_c = Contrast(rows, "B", "C", 1)
    report_contrast(vs_a)
    report_contrast(vs_c)
    passed = evaluate(label, vs_a, vs_c)
    return passed, (vs_a, vs_c)


# ---------------------------------------------------------------- self-test


def synthetic() -> list:
    """B beats A on yield while tying C, at quality parity.

    The planted answer is "the gain is compute, not scheme" -- the
    discrimination 2026-07-31-replicate could not make and
    2026-08-10-arbiter-probe had no arm for.
    """
    rng = random.Random(7)
    rows = []
    yields = {"A": 0.55, "B": 0.75, "C": 0.75}
    for spec in ("s1", "s2", "s3", "s4"):
        for seed in range(200):
            for arm in ("A", "B", "C"):
                found = rng.random() < yields[arm]
                rows.append({
                    "level_name": "cm-elit0.1" if arm == "C" else "elit0.1",
                    "selection": ("nsga2-apportion" if arm == "B"
                                  else "nsga2-truncate"),
                    "spec": spec, "seed": str(seed),
                    "found_repair": "1" if found else "0",
                    "n_repairs": str(rng.randint(1, 4) if found else 0),
                    "implies_ideal": f"{rng.random():.4f}",
                    "wall_time_s": f"{rng.uniform(9, 11):.2f}",
                    "compare_timed_out": "0",
                })
    for row in rows:
        row["arm"] = arm_of(row)
    return rows


def self_test() -> int:
    rows = synthetic()
    passed, (vs_a, vs_c) = analyse(rows, "SELF-TEST (synthetic)")
    failures = []
    if not vs_a.odds_lo > 1.0:
        failures.append("planted B>A yield gain not recovered")
    if vs_c.odds_lo > 1.0:
        failures.append("spurious B>C yield gain")
    if passed:
        failures.append("verdict passed on a dataset that ties arm C")
    print()
    for f in failures:
        print(f"   SELF-TEST FAILURE: {f}")
    print("   self-test: "
          f"{'recovers the planted effect' if not failures else 'BROKEN'}")
    return 1 if failures else 0


# ---------------------------------------------------------------- main


def main() -> int:
    args = parse_args()
    if args.self_test:
        return self_test()
    if not args.fretish or not args.tlsf:
        print("both --fretish and --tlsf are required (the paths are analysed "
              "separately but the decision rule spans them)")
        return 2

    results = {}
    contrasts = {}
    for label, path in (("FRETISH", args.fretish), ("TLSF", args.tlsf)):
        passed, pair = analyse(load(path), label)
        results[label] = passed
        contrasts[label] = pair

    deliverable(results["FRETISH"], results["TLSF"], contrasts)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
