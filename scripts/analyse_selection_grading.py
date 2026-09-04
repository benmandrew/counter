#!/usr/bin/env python3
"""Analyse the 2026-08-28 selection-scheme x status-grading ablation.

Reads the merged results CSV and the curve CSV `score_curves.py` writes, runs
the two checks PLAN.md section 12 registers, and prints the wall-time picture
and the anytime curves for the 2x2.

NO DECISION RULE. PLAN.md section 2 registers no primary endpoint, no alpha and
no control arm, so every p-value printed here is POST-HOC: it describes this
sample and tests no hypothesis, the contrast and the metric and the time cut
all having been free when they were picked. Every block that computes one says
so on the line above it.

Two things bound what the selection contrast can claim, and both were written
down before the run:

  * The weights ride with two of the four arms by construction. AuRUS's
    published triple (0.1 syntactic, 0.2 semantic, 0.7 status) is set globally,
    and only the `weighted` scheme reads it; both NSGA-II arms rank the three
    objectives separately and ignore it. So the selection factor is NSGA-II
    against AuRUS-weighted *scalarisation*, not NSGA-II against weighting in
    general (PLAN.md section 7).
  * Censoring is heavy and uneven. 245 of 600 runs hit the 3600 s external cap
    and wrote no manifest, so their `implies_ideal` reads zero in the results
    CSV by construction. Only the accumulator scores what those runs actually
    found, which is why the curve metrics and the endpoint disagree and why
    both are printed.

Usage:
    python3 scripts/analyse_selection_grading.py [--curves PATH] [--results PATH]
"""

import argparse
import collections
import json
import math
import statistics
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from analysis_lib import (  # noqa: E402
    load_rows,
    mcnemar_exact,
    med,
    num_or_nan,
    pct,
    signed_ranks,
)

REPO_ROOT = Path(__file__).resolve().parent.parent
RESULTS_CSV = REPO_ROOT / "experiments" / "results-curves.csv"
CURVES_CSV = REPO_ROOT / "experiments" / "curves-grading.csv"
RUNS_DIR = REPO_ROOT / "experiments" / "results-curves"

SELECTIONS = ("nsga2-apportion", "weighted")
GRADINGS = ("mrs", "aurus")
ARMS = tuple(f"{s}/{g}" for s in SELECTIONS for g in GRADINGS)

# The internal deadline every run was given, and the external cap the harness
# killed it at. Both are facts about the launch.
DEADLINE_S = 400.0
HARNESS_CAP_S = 3600.0

# Log-spaced, and stopping at the external cap because no run has data past it.
TIME_CUTS = (1, 2, 5, 10, 20, 50, 100, 200, 400, 800, 1600, 3600)

POSTHOC = "POST-HOC (PLAN section 2 registers no decision rule)"


def rule(title):
    print(f"\n{'=' * 78}\n{title}\n{'=' * 78}")


def sub(title):
    print(f"\n-- {title}")


def wilcoxon_exact_p(differences):
    """Two-sided exact signed-rank p over the observed ranks.

    analysis_lib ships no tail deliberately, an exact enumeration and a normal
    approximation answering different questions at different sample sizes. At
    two dozen families the exact one is cheap, counted over subsets of the
    doubled ranks rather than over 2**n sign assignments.
    """
    w_plus, ranks, _ = signed_ranks(differences)
    if not ranks:
        return 1.0
    doubled = [int(round(2 * r)) for r in ranks]
    total = sum(doubled)
    counts = [0] * (total + 1)
    counts[0] = 1
    for rank in doubled:
        for value in range(total, rank - 1, -1):
            counts[value] += counts[value - rank]
    observed = int(round(2 * w_plus))
    lower = min(observed, total - observed)
    tail = sum(counts[:lower + 1]) + sum(counts[total - lower:])
    return min(1.0, tail / float(2 ** len(doubled)))


def arm_of(row):
    return f"{row['selection']}/{row['level_value']}"


def load_results(path):
    rows = [r for r in load_rows(path) if r.get("spec") and r["spec"] != "spec"]
    for row in rows:
        row["arm"] = arm_of(row)
    return rows


def load_curves(path):
    """{(spec, seed, arm): {metric: [(t, value)]}} plus the scalars."""
    if not Path(path).exists():
        return None, None, None
    curves = collections.defaultdict(lambda: collections.defaultdict(list))
    unknown = set()
    scalars = {}
    for row in load_rows(path):
        if not row.get("spec"):
            continue
        arm = f"{row['selection_scheme']}/{row['status_grading']}"
        key = (row["spec"], int(row["seed"]), arm)
        metric = row["metric"]
        if row["elapsed_s"] == "":
            scalars[(key, metric)] = (
                num_or_nan(row["value"]),
                None if row["censored"] == "" else int(row["censored"]))
        else:
            value = num_or_nan(row["value"])
            if math.isnan(value):
                unknown.add((key, metric))
                curves[key].setdefault(metric, [])
            else:
                curves[key][metric].append((float(row["elapsed_s"]), value))
    for key, _ in list(scalars):
        if scalars.get((key, "time_to_first_ideal_repair"), (None, 0))[1] is None:
            unknown.add((key, "ideal_solutions"))
            unknown.add((key, "maximal_ideal_solutions"))
    for metrics in curves.values():
        for points in metrics.values():
            points.sort()
    return curves, scalars, unknown


def curves_from_index(runs_dir):
    """Derive `solutions` and `time_to_first_repair` from the accumulator alone.

    Two of the six metrics need no solver call, and on this campaign they are
    the ones that matter most: 245 of 600 runs were killed at the external cap
    and their `implies_ideal` reads zero in the results CSV whatever they
    found, while the accumulator flushed every gate-passing candidate as it
    went. The endpoint says the weighted arms found almost nothing; this says
    what they actually found before they were killed.
    """
    sys.path.insert(0, str(Path(__file__).parent))
    from score_curves import (cumulative_points, read_index,
                              spec_from_dir_name, seed_from_dir_name,
                              read_config)
    curves = collections.defaultdict(lambda: collections.defaultdict(list))
    scalars = {}
    for run_dir in sorted(Path(runs_dir).iterdir()):
        if not run_dir.is_dir():
            continue
        config = read_config(run_dir)
        arm = (f"{config.get('selection_scheme', '')}/"
               f"{config.get('status_grading', '')}")
        seed = seed_from_dir_name(run_dir)
        if arm == "/" or not seed:
            continue
        index = read_index(run_dir)
        key = (spec_from_dir_name(run_dir), int(seed), arm)
        times = sorted(row[2] for row in index)
        curves[key]["solutions"] = cumulative_points(times)
        scalars[(key, "time_to_first_repair")] = (
            times[0] if times else float("nan"), int(not times))
    return curves, scalars, set()


def step_at(points, moment):
    value = 0.0
    for time, count in points:
        if time > moment:
            break
        value = count
    return value


def integrity(results, curves):
    rule("INTEGRITY")
    print(f"results rows          {len(results)}")
    for (commit, dirty), n in collections.Counter(
            (r["commit"], r["dirty"]) for r in results).most_common():
        print(f"  commit {commit} dirty={dirty}: {n} rows")
    specs = sorted({r["spec"] for r in results})
    seeds = sorted({int(r["seed"]) for r in results})
    print(f"families {len(specs)}, seeds {len(seeds)} "
          f"({min(seeds)}-{max(seeds)})")
    by_arm = collections.Counter(r["arm"] for r in results)
    for arm in ARMS:
        print(f"  {arm:28s} {by_arm.get(arm, 0)} rows")
    if curves is None:
        print("curves                ABSENT -- curve sections are skipped")
        return
    covered, wanted = set(curves), {
        (r["spec"], int(r["seed"]), r["arm"]) for r in results}
    print(f"curve runs covered    {len(covered)} of {len(wanted)} "
          f"({pct(len(covered), len(wanted))})")
    if covered != wanted:
        by = collections.Counter(arm for _, _, arm in covered)
        print("  PARTIAL -- read a per-arm figure only where the arms are "
              "covered alike:")
        for arm in ARMS:
            want = sum(1 for _, _, a in wanted if a == arm)
            print(f"    {arm:28s} {by.get(arm, 0):4d} of {want}")


def verification(results):
    """PLAN.md section 12: the two things that would make these unreadable."""
    rule("VERIFICATION -- the two checks of PLAN section 12")
    manifests, censored = [], []
    for run_dir in sorted(RUNS_DIR.iterdir()):
        if not run_dir.is_dir():
            continue
        path = run_dir / "run.json"
        if path.exists():
            try:
                manifests.append(json.loads(path.read_text()))
            except json.JSONDecodeError:
                print(f"  WARN: {run_dir.name}/run.json is not JSON")
        elif (run_dir / "accumulated" / "index.tsv").exists():
            censored.append(run_dir.name)
    print(f"manifests {len(manifests)}, censored directories {len(censored)}")

    sub("1. ideal_solutions must not be flat zero across every arm")
    implies = sum(1 for r in results if r["implies_ideal"] == "1")
    print(f"   results CSV implies_ideal: {implies} of {len(results)} "
          f"({pct(implies, len(results))})")
    for arm in ARMS:
        sub_rows = [r for r in results if r["arm"] == arm]
        hits = sum(1 for r in sub_rows if r["implies_ideal"] == "1")
        print(f"     {arm:28s} {hits}/{len(sub_rows)} ({pct(hits, len(sub_rows))})")
    print("   PASS: no arm is flat zero" if implies else
          "   FAIL: check the compare join before the search")

    sub("2. stopped_by must read `deadline` on essentially every row")
    stopped = collections.Counter(m.get("stopped_by") for m in manifests)
    for value, n in stopped.most_common():
        print(f"   {str(value):14s} {n}")
    if stopped.get("generations"):
        print("   FAIL: a family reached 500 generations inside 400s, so the "
              "time axis is not shared")
    else:
        print("   PASS: the deadline ended every run that wrote a manifest, "
              "so the arms share a time axis")

    sub("censoring, which PLAN section 6 anticipated")
    print(f"   censored {len(censored)} of {len(results)} "
          f"({pct(len(censored), len(results))}), at the {HARNESS_CAP_S:.0f}s "
          f"external cap")
    by_family = collections.Counter(
        name.split("_wkoff_log_")[-1].rsplit("_seed", 1)[0]
        for name in censored)
    for family, n in by_family.most_common(8):
        print(f"     {family:32s} {n}")
    print("   A censored run wrote no manifest, so its implies_ideal reads "
          "zero in the results CSV whatever it found.")
    return censored


def cost(results):
    rule("COST")
    walls = collections.defaultdict(list)
    for row in results:
        value = num_or_nan(row["wall_time_s"])
        if not math.isnan(value):
            walls[row["arm"]].append(value)
    sub("wall time by arm")
    for arm in ARMS:
        values = walls.get(arm, [])
        killed = sum(1 for r in results
                     if r["arm"] == arm and r["timed_out"] == "1")
        print(f"   {arm:28s} mean {statistics.mean(values):7.1f}s  "
              f"median {med(values):>6}s  killed {killed}/{len(values)}")
    print(f"\n   Every run was given a {DEADLINE_S:.0f}s internal deadline and "
          f"a {HARNESS_CAP_S:.0f}s external cap.")
    print("   The gap between them is the post-deadline work the deadline does "
          "not bound: the final")
    print("   realizability gate, the implication filter and the scoring, none "
          "of which it interrupts.")


def curve_table(curves, metric, unknown):
    sub(f"{metric}: mean per run at each cut")
    computed = metric.startswith("maximal")
    scored = {arm: [k for k, m in curves.items()
                    if k[2] == arm and (k, metric) not in unknown
                    and (metric in m or not computed)] for arm in ARMS}
    dropped = {arm: sum(1 for k in curves
                        if k[2] == arm and (k, metric) in unknown)
               for arm in ARMS}
    print("   cut(s)  " + "".join(f"{arm:>22s}" for arm in ARMS))
    print("   runs    " + "".join(f"{len(scored[a]):>22d}" for a in ARMS))
    if any(dropped.values()):
        print("   undecided" + "".join(f"{dropped[a]:>21d}" for a in ARMS))
    for moment in TIME_CUTS:
        cells = []
        for arm in ARMS:
            values = [step_at(curves[k].get(metric, []), moment)
                      for k in scored[arm]]
            cells.append(f"{statistics.mean(values):22.2f}" if values
                         else f"{'n/a':>22s}")
        print(f"   {moment:6d}  " + "".join(cells))


def curve_sections(curves, unknown):
    if curves is None:
        rule("CURVES -- SKIPPED, no curve CSV")
        return
    rule("CURVES -- by arm")
    print("Mean count per run at each cut, carrying a stopped run's last value")
    print("forward: what a run found by time t is what it had when it stopped.")
    for metric in ("solutions", "ideal_solutions",
                   "maximal_solutions", "maximal_ideal_solutions"):
        if any(metric in m for m in curves.values()):
            curve_table(curves, metric, unknown)
        else:
            print(f"\n-- {metric}: absent from the curve CSV")


def discovery(scalars):
    rule("DISCOVERY -- time to first repair, and to first ideal repair")
    if scalars is None:
        print("curve CSV absent; skipped")
        return
    for metric in ("time_to_first_repair", "time_to_first_ideal_repair"):
        sub(metric)
        for arm in ARMS:
            reached, n = [], 0
            for (key, name), (value, flag) in scalars.items():
                if name != metric or key[2] != arm or flag is None:
                    continue
                n += 1
                if flag == 0 and not math.isnan(value):
                    reached.append(value)
            shown = f"{statistics.median(reached):.1f}s" if reached else "n/a"
            print(f"   {arm:28s} reached {len(reached)}/{n} "
                  f"({pct(len(reached), n)})  median among reachers {shown}")


def contrast(results, name, left, right, pick):
    pairs = collections.defaultdict(dict)
    for row in results:
        side = ("left" if row["selection"] == left or row["level_value"] == left
                else "right" if row["selection"] == right
                or row["level_value"] == right else None)
        if side:
            pairs[(row["spec"], row["seed"], pick(row))][side] = row
    complete = [v for v in pairs.values() if len(v) == 2]
    print(f"\n   {name}: {left} against {right}, {len(complete)} pairs")
    print(f"   {POSTHOC}")
    for column in ("found_repair", "implies_ideal"):
        b = sum(1 for v in complete
                if v["left"][column] == "1" and v["right"][column] != "1")
        c = sum(1 for v in complete
                if v["left"][column] != "1" and v["right"][column] == "1")
        print(f"     {column:16s} {left} only {b:4d}, {right} only {c:4d}, "
              f"exact McNemar p = {mcnemar_exact(b, c):.4f}")
    killed = {side: sum(1 for v in complete if v[side]["timed_out"] == "1")
              for side in ("left", "right")}
    print(f"     killed at the cap {left} {killed['left']}, "
          f"{right} {killed['right']} of {len(complete)}")


def posthoc(results):
    rule("POST-HOC FACTOR CONTRASTS -- the 2x2, read after the fact")
    print("PLAN section 2 registers no decision rule and no primary endpoint.")
    print("Nothing below tests a hypothesis; it describes this sample.")
    print("PLAN section 7: the selection contrast is NSGA-II against")
    print("AuRUS-weighted scalarisation, the weights riding with two arms by")
    print("construction, not NSGA-II against weighting in general.")
    contrast(results, "selection", "nsga2-apportion", "weighted",
             lambda r: r["level_value"])
    contrast(results, "status grading", "mrs", "aurus",
             lambda r: r["selection"])

    sub("per-family implies_ideal, all four arms")
    by = collections.defaultdict(lambda: collections.defaultdict(list))
    for row in results:
        by[row["spec"]][row["arm"]].append(
            1 if row["implies_ideal"] == "1" else 0)
    print("   " + f"{'family':30s}" + "".join(f"{a:>22s}" for a in ARMS))
    for family in sorted(by):
        cells = []
        for arm in ARMS:
            v = by[family].get(arm, [])
            cells.append(f"{sum(v)}/{len(v)}".rjust(22) if v
                         else "n/a".rjust(22))
        print(f"   {family:30s}" + "".join(cells))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", default=RESULTS_CSV)
    parser.add_argument("--curves", default=CURVES_CSV)
    parser.add_argument("--from-index", action="store_true",
                        help="derive solutions and time_to_first_repair from "
                             "the accumulator alone, with no solver call")
    args = parser.parse_args()

    results = load_results(args.results)
    if args.from_index:
        curves, scalars, unknown = curves_from_index(RUNS_DIR)
        print("SOLVER-FREE READ: solutions and time_to_first_repair only.")
    else:
        curves, scalars, unknown = load_curves(args.curves)

    integrity(results, curves)
    verification(results)
    cost(results)
    curve_sections(curves, unknown)
    discovery(scalars)
    posthoc(results)

    rule("READING THESE NUMBERS")
    print("Every p-value above is post-hoc; PLAN section 2 registers no rule.")
    print("The weights ride with the two weighted arms by construction, so the")
    print("selection factor is NSGA-II against AuRUS-weighted scalarisation.")
    print("245 of 600 runs were killed at the cap and read implies_ideal zero")
    print("whatever they found; only the accumulator curves score those.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
