#!/usr/bin/env python3
"""Scores `ops-grammar` and `ops-weakening` against `experiments/ops-grammar/PLAN.md`.

Two campaigns, one design. `ops-grammar` crosses the repaired mutation and
crossover grammar (`repaired_operators`, sweep O, levels `opslegacy` and
`opsfixed`) with the weakening screen held on, which is what every archived
campaign ran. `ops-weakening` is the same cross with `run_weakening = false`,
the default the binary is about to take. Section 8 of the plan registered the
screen as a limit on how far the first campaign's result carries and named the
second as the replication that decides what ships.

The primary endpoint is section 5's: the paired per-run `implies_ideal`, exact
McNemar over the discordant pairs, computed separately per path and never
pooled -- the two paths score against different ideal sets, so a pooled rate is
not a rate of anything. Section 9's rule is evaluated as amended by 9a, whose
wall-ratio bound is per path and gates the path that won.

The 2x2 is the reason both campaigns ran the same seeds over the same corpus.
It is reported for what it is: an unrandomised assembly of two campaigns run at
two commits, so the weakening contrast down each column is between campaigns
rather than within one, and the interaction is descriptive.
"""

import argparse
import csv
import math
import statistics
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
EXPERIMENTS = REPO_ROOT / "experiments"

CONTROL = "opslegacy"
TREATMENT = "opsfixed"

# Section 9 outcome 1, as amended by 9a: per path, gating the path that won.
WALL_RATIO_BOUND = 1.25
ALPHA = 0.05

# Each CSV is looked for in the campaign's archive directory first and in a
# flat experiments/ second, so the script reads the same data before and after
# the campaign is closed.
DATASETS = [
    ("wkon", "fret", "2026-08-20-ops-grammar", "results-ops-fret.csv"),
    ("wkon", "tlsf", "2026-08-20-ops-grammar", "results-ops-tlsf.csv"),
    ("wkoff", "fret", "2026-08-20-ops-weakening", "results-opswk-fret.csv"),
    ("wkoff", "tlsf", "2026-08-20-ops-weakening", "results-opswk-tlsf.csv"),
]

SCREEN = {"wkon": "run_weakening = true", "wkoff": "run_weakening = false"}


def load(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def number(value):
    return float(value) if value not in ("", None) else float("nan")


def flag(value):
    return int(value) == 1


def pair_up(rows):
    """Index rows by (spec, seed), returning only the fully paired cases."""
    by_key = {}
    for row in rows:
        by_key.setdefault((row["spec"], row["seed"]), {})[row["level_value"]] = row
    pairs, unpaired = {}, []
    for key, arms in sorted(by_key.items()):
        if CONTROL in arms and TREATMENT in arms:
            pairs[key] = arms
        else:
            unpaired.append(key)
    return pairs, unpaired


def binom_cdf(k, n):
    if k < 0:
        return 0.0
    total = sum(math.comb(n, i) for i in range(0, min(k, n) + 1))
    return total / (2.0**n)


def mcnemar_exact(b, c):
    """Two-sided exact McNemar over b treatment-only and c control-only wins."""
    n = b + c
    if n == 0:
        return 1.0
    return min(1.0, 2.0 * binom_cdf(min(b, c), n))


def median(values):
    return statistics.median(values) if values else float("nan")


class Contrast:
    """`opsfixed` against `opslegacy` on one path of one campaign."""

    def __init__(self, screen, path, rows):
        self.screen = screen
        self.path = path
        self.rows = rows
        self.pairs, self.unpaired = pair_up(rows)

        self.both = self.treatment_only = self.control_only = self.neither = 0
        self.wall_ratios = []
        self.per_spec = {}
        for (spec, _), arms in self.pairs.items():
            fixed = flag(arms[TREATMENT]["implies_ideal"])
            legacy = flag(arms[CONTROL]["implies_ideal"])
            cell = self.per_spec.setdefault(spec, [0, 0, 0, 0])
            if fixed and legacy:
                self.both += 1
                cell[0] += 1
            elif fixed:
                self.treatment_only += 1
                cell[1] += 1
            elif legacy:
                self.control_only += 1
                cell[2] += 1
            else:
                self.neither += 1
                cell[3] += 1
            control_wall = number(arms[CONTROL]["wall_time_s"])
            if control_wall > 0:
                self.wall_ratios.append(
                    number(arms[TREATMENT]["wall_time_s"]) / control_wall)

        self.p_value = mcnemar_exact(self.treatment_only, self.control_only)
        self.wall_ratio = median(self.wall_ratios)

    @property
    def name(self):
        return f"{self.path}/{self.screen}"

    @property
    def n_pairs(self):
        return len(self.pairs)

    def rate(self, level, column):
        values = [flag(row[column]) for row in self.rows
                  if row["level_value"] == level]
        return sum(values) / len(values) if values else float("nan")

    def mean(self, level, column):
        values = [number(row[column]) for row in self.rows
                  if row["level_value"] == level]
        return statistics.fmean(values) if values else float("nan")

    def relations(self, level):
        counts = {}
        for row in self.rows:
            if row["level_value"] == level:
                counts[row["best_relation"]] = counts.get(row["best_relation"], 0) + 1
        return counts

    def wins(self):
        """Which arm the endpoint favours, or None where p >= alpha."""
        if self.p_value >= ALPHA:
            return None
        return TREATMENT if self.treatment_only > self.control_only else CONTROL

    def triggers_outcome_1(self):
        return self.wins() == TREATMENT and self.wall_ratio < WALL_RATIO_BOUND


def report_contrast(contrast):
    print(f"\n--- {contrast.name}  ({SCREEN[contrast.screen]}) "
          f"{'-' * max(0, 44 - len(contrast.name))}")
    if contrast.unpaired:
        print(f"  UNPAIRED (excluded): {len(contrast.unpaired)} "
              f"(spec, seed) cases: {contrast.unpaired[:6]}")
    print(f"  pairs: {contrast.n_pairs}   rows: {len(contrast.rows)}")

    print("\n  Primary endpoint -- paired implies_ideal, exact McNemar")
    print(f"    both arms         {contrast.both:4d}")
    print(f"    {TREATMENT} only  {contrast.treatment_only:4d}   (discordant, treatment)")
    print(f"    {CONTROL} only    {contrast.control_only:4d}   (discordant, control)")
    print(f"    neither arm       {contrast.neither:4d}")
    print(f"    p = {contrast.p_value:.4f}   "
          f"({'significant' if contrast.p_value < ALPHA else 'not significant'} "
          f"at {ALPHA})")
    legacy_rate = (contrast.both + contrast.control_only) / contrast.n_pairs
    fixed_rate = (contrast.both + contrast.treatment_only) / contrast.n_pairs
    print(f"    implies_ideal rate  {CONTROL} {legacy_rate:6.1%}   "
          f"{TREATMENT} {fixed_rate:6.1%}   "
          f"({fixed_rate - legacy_rate:+.1%})")

    print("\n  Reported, not decisive")
    for level in (CONTROL, TREATMENT):
        print(f"    {level:10s} yield {contrast.rate(level, 'found_repair'):6.1%}   "
              f"n_repairs {contrast.mean(level, 'n_repairs'):5.2f}   "
              f"timed_out {contrast.rate(level, 'timed_out'):5.1%}   "
              f"wall {contrast.mean(level, 'wall_time_s'):7.1f}s")
    for level in (CONTROL, TREATMENT):
        relations = contrast.relations(level)
        rendered = "  ".join(f"{k} {v}" for k, v in sorted(relations.items()))
        print(f"    {level:10s} best_relation: {rendered}")
    ratios = sorted(contrast.wall_ratios)
    if ratios:
        print(f"    median paired wall ratio {contrast.wall_ratio:.3f}   "
              f"(min {ratios[0]:.3f}, max {ratios[-1]:.3f}, "
              f"bound {WALL_RATIO_BOUND})")

    print("\n  Per spec (both / fixed-only / legacy-only / neither)")
    for spec, cell in sorted(contrast.per_spec.items()):
        discordant = mcnemar_exact(cell[1], cell[2])
        print(f"    {spec:16s} {cell[0]:3d} {cell[1]:3d} {cell[2]:3d} {cell[3]:3d}"
              f"   p={discordant:.3f}")


def report_2x2(path, wkon, wkoff):
    """The grammar x weakening assembly, on the (spec, seed) pairs both ran."""
    shared = sorted(set(wkon.pairs) & set(wkoff.pairs))
    print(f"\n=== 2x2 assembly, {path} path "
          f"({len(shared)} (spec, seed) cases in both campaigns) ===")
    if not shared:
        print("  no shared cases -- the campaigns did not run the same cross")
        return

    cells = {}
    for level in (CONTROL, TREATMENT):
        for screen, contrast in (("wkon", wkon), ("wkoff", wkoff)):
            hits = sum(flag(contrast.pairs[key][level]["implies_ideal"])
                       for key in shared)
            cells[(level, screen)] = hits

    print(f"\n  implies_ideal over {len(shared)} runs per cell")
    print(f"    {'':12s} {'wkon':>14s} {'wkoff':>14s}")
    for level in (CONTROL, TREATMENT):
        on, off = cells[(level, "wkon")], cells[(level, "wkoff")]
        print(f"    {level:12s} {on:5d} {on / len(shared):7.1%} "
              f"{off:5d} {off / len(shared):7.1%}")

    print("\n  Weakening screen off vs on, within each grammar arm")
    print("  The direction here is near-structural rather than discovered, so")
    print("  the counts are the finding and no p-value is quoted: the screen is")
    print("  a FINAL filter drawing nothing from the RandomSource, so both")
    print("  campaigns' matched runs search identically and turning it off can")
    print("  essentially only admit repairs. Not a strict superset, though --")
    print("  the implication filter runs after it, so a newly admitted")
    print("  non-weakening can dominate and displace several weakenings.")
    for level in (CONTROL, TREATMENT):
        off_only = on_only = 0
        for key in shared:
            off = flag(wkoff.pairs[key][level]["implies_ideal"])
            on = flag(wkon.pairs[key][level]["implies_ideal"])
            off_only += off and not on
            on_only += on and not off
        print(f"    {level:12s} gained {off_only:3d}   lost {on_only:3d}"
              f"   of {len(shared)} runs")

    shrank = sum(1 for key in shared for level in (CONTROL, TREATMENT)
                 if int(wkoff.pairs[key][level]["n_repairs"])
                 < int(wkon.pairs[key][level]["n_repairs"]))
    print(f"    n_repairs fell with the screen off in {shrank} of "
          f"{2 * len(shared)} matched runs (the displacement above)")

    interaction = ((cells[(TREATMENT, "wkoff")] - cells[(CONTROL, "wkoff")])
                   - (cells[(TREATMENT, "wkon")] - cells[(CONTROL, "wkon")]))
    print(f"\n  grammar effect  wkon {cells[(TREATMENT, 'wkon')] - cells[(CONTROL, 'wkon')]:+d}"
          f"   wkoff {cells[(TREATMENT, 'wkoff')] - cells[(CONTROL, 'wkoff')]:+d}"
          f"   interaction {interaction:+d} runs")


def report_per_spec_2x2(path, wkon, wkoff):
    """Where the pooled endpoint comes from, family by family.

    The plan registered no per-spec test and these p-values are exploratory,
    but the pooled figure is a sum of families pulling in both directions and
    reporting only the sum would hide that.
    """
    shared = sorted(set(wkon.pairs) & set(wkoff.pairs))
    specs = sorted({spec for spec, _ in shared})
    print(f"\n  Per spec, implies_ideal hits per cell, {path} path")
    print(f"    {'spec':16s} {'legacy/on':>10s} {'fixed/on':>10s} "
          f"{'legacy/off':>11s} {'fixed/off':>10s}   grammar on / off")
    for spec in specs:
        keys = [key for key in shared if key[0] == spec]
        n = len(keys)
        cells = {}
        for level in (CONTROL, TREATMENT):
            for screen, contrast in (("on", wkon), ("off", wkoff)):
                cells[(level, screen)] = sum(
                    flag(contrast.pairs[key][level]["implies_ideal"])
                    for key in keys)
        rendered = []
        for contrast in (wkon, wkoff):
            b = sum(1 for key in keys
                    if flag(contrast.pairs[key][TREATMENT]["implies_ideal"])
                    and not flag(contrast.pairs[key][CONTROL]["implies_ideal"]))
            c = sum(1 for key in keys
                    if flag(contrast.pairs[key][CONTROL]["implies_ideal"])
                    and not flag(contrast.pairs[key][TREATMENT]["implies_ideal"]))
            rendered.append(f"{b:2d}v{c:<2d} p={mcnemar_exact(b, c):.3f}")
        print(f"    {spec:16s} {cells[(CONTROL, 'on')]:6d}/{n:<3d} "
              f"{cells[(TREATMENT, 'on')]:6d}/{n:<3d} "
              f"{cells[(CONTROL, 'off')]:7d}/{n:<3d} "
              f"{cells[(TREATMENT, 'off')]:6d}/{n:<3d}   {'  '.join(rendered)}")


def report_decision(contrasts):
    """Section 9, read against `ops-weakening` -- the campaign that decides."""
    print("\n" + "=" * 72)
    print("Decision rule -- ops-grammar/PLAN.md section 9, as amended by 9a")
    print("=" * 72)

    for screen in ("wkon", "wkoff"):
        arms = [c for c in contrasts if c.screen == screen]
        print(f"\n  {SCREEN[screen]}  "
              f"({'ops-grammar' if screen == 'wkon' else 'ops-weakening'})")
        for contrast in arms:
            winner = contrast.wins()
            verdict = winner or "no significant difference"
            gate = ("ratio passes" if contrast.wall_ratio < WALL_RATIO_BOUND
                    else "ratio FAILS")
            print(f"    {contrast.path:5s} p={contrast.p_value:.4f}  "
                  f"{verdict:26s} wall {contrast.wall_ratio:.3f} ({gate})")

        treatment_wins = [c for c in arms if c.wins() == TREATMENT]
        control_wins = [c for c in arms if c.wins() == CONTROL]
        outcome_1 = [c for c in arms if c.triggers_outcome_1()]
        if control_wins and not treatment_wins:
            outcome = "4 -- the repaired arm loses: revert"
        elif control_wins and treatment_wins:
            outcome = "2 -- one path each way: do not flip, report per path"
        elif outcome_1:
            outcome = ("1 -- flip the default to true "
                       f"(on {', '.join(c.path for c in outcome_1)})")
        elif treatment_wins:
            outcome = ("1 not met -- a path wins its endpoint but fails its "
                       "own wall-ratio bound; no outcome flips the default")
        else:
            outcome = "3 -- no significant difference: do not flip"
        print(f"    => outcome {outcome}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--experiments", type=Path, default=EXPERIMENTS,
                        help="Directory holding the four results CSVs.")
    args = parser.parse_args()

    contrasts = []
    for screen, path, archive, name in DATASETS:
        for candidate in (args.experiments / archive / name,
                          args.experiments / name):
            if candidate.exists():
                break
        else:
            raise SystemExit(f"missing results CSV: {name} (looked in "
                             f"{args.experiments / archive} and "
                             f"{args.experiments})")
        contrasts.append(Contrast(screen, path, load(candidate)))

    print("=" * 72)
    print("ops-grammar and ops-weakening -- sweep O, opslegacy vs opsfixed")
    print("=" * 72)
    for contrast in contrasts:
        report_contrast(contrast)

    by_path = {}
    for contrast in contrasts:
        by_path.setdefault(contrast.path, {})[contrast.screen] = contrast
    for path in ("fret", "tlsf"):
        cells = by_path.get(path, {})
        if "wkon" in cells and "wkoff" in cells:
            report_2x2(path, cells["wkon"], cells["wkoff"])
            report_per_spec_2x2(path, cells["wkon"], cells["wkoff"])

    report_decision(contrasts)


if __name__ == "__main__":
    main()
