#!/usr/bin/env python3
"""Analyse the 2026-08-29 individuals-matched campaign.

Reads the merged results CSV, the curve CSV `score_curves.py` writes, and the
AuRUS reference extracted from that tool's own archive, and prints the
verification checks, the wall-time picture and the anytime curves.

NO DECISION RULE. `experiments/2026-08-29-aurus-matched/PLAN.md` section 2
registers no primary endpoint, no alpha and no control arm, so every p-value
printed here is POST-HOC: it describes this sample and tests no hypothesis, the
contrast and the metric and the time cut all having been free when they were
picked. Every block that computes one says so on the line above it, and that
labelling has to survive into REPORT.md and PROVENANCE.json.

Two asymmetries between the tools bound what any of this can say, and neither
is a defect to be corrected away:

  * The two sides do not mean the same thing by "a solution". counter's curve
    counts candidates that passed its output gate, flushed by the accumulator
    with sub-second timestamps. AuRUS's counts the running length of
    `ga.solutions` read off its per-iteration log line, which moves only at a
    generation boundary and is dated to the second. counter's curve therefore
    has resolution AuRUS's cannot, and a like-for-like reading has to be taken
    at cuts coarse enough for both.
  * A killed run is not symmetric either. AuRUS writes its solution files in
    one batch when the run ends, so a run killed at the cap loses every one of
    them and can carry no verdict; counter's accumulator flushes as it goes, so
    a killed counter run keeps every candidate it found. Those rows are marked
    `lost-at-cap` on the AuRUS side and are excluded from any verdict rate,
    which biases that rate towards the runs that finished.

The ideal rates on the AuRUS side are UNFILTERED. `analyse_aurus_h2h.py` scored
a well-separation-screened `implies_genuine` from `validation-av2.csv` and
`validation-av3.csv`; neither file survives on either host, so the `hit` column
is used as-is. The archived record of that screen is that it moved 2 of 25
family rates -- lift 0.167 to 0.300, lily11 0.900 to 0.833 -- so it is a small
correction in an unknown direction rather than a negligible one.

Usage:
    python3 scripts/analyse_matched.py [--curves PATH] [--results PATH]
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
RESULTS_CSV = REPO_ROOT / "experiments" / "results-matched.csv"
CURVES_CSV = REPO_ROOT / "experiments" / "curves-matched.csv"
RUNS_DIR = REPO_ROOT / "experiments" / "results-matched"
AURUS_DIR = REPO_ROOT / "experiments" / "aurus-reference"

SELECTIONS = ("nsga2-apportion", "weighted")
GRADINGS = ("mrs", "aurus")
ARMS = tuple(f"{s}/{g}" for s in SELECTIONS for g in GRADINGS)

# The individuals cap the campaign ran under, and the harness cap that bounds a
# run from outside. Both are facts about the launch, not about these rows.
MAX_INDIVIDUALS = 1000
HARNESS_CAP_S = 7200.0

# Cuts for the shared time axis. Log-spaced because the first minute of a run
# holds most of what either search will ever find, and stopping at the harness
# cap because neither tool has data past its own kill.
TIME_CUTS = (1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 7200)

POSTHOC = "POST-HOC (PLAN section 2 registers no decision rule)"


def wilcoxon_exact_p(differences):
    """Two-sided exact signed-rank p over the observed ranks.

    analysis_lib deliberately ships no tail: an exact enumeration and a normal
    approximation answer different questions at different sample sizes, so each
    analyser keeps the one its sample warrants. Two dozen families is small
    enough to enumerate, which is done by counting subsets of the ranks that
    reach each sum rather than over 2**n sign assignments. Ranks are half
    integers where absolute differences tie, so they are doubled first.
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
    space = float(2 ** len(doubled))
    observed = int(round(2 * w_plus))
    lower = min(observed, total - observed)
    tail = sum(counts[:lower + 1]) + sum(counts[total - lower:])
    return min(1.0, tail / space)


def rule(title):
    print(f"\n{'=' * 78}\n{title}\n{'=' * 78}")


def sub(title):
    print(f"\n-- {title}")


# -- loading -------------------------------------------------------------------


def arm_of(row):
    return f"{row['selection']}/{row['level_value']}"


def load_results(path):
    rows = [r for r in load_rows(path) if r.get("spec") and r["spec"] != "spec"]
    for row in rows:
        row["arm"] = arm_of(row)
        row["key"] = (row["spec"], int(row["seed"]))
    return rows


def load_curves(path):
    """Curve rows grouped as {(spec, seed, arm): {metric: [(t, value)]}}.

    The scalar metrics land in the same structure under a single point whose
    time is the event time, so a caller reads them from `scalars` rather than
    from here.
    """
    if not Path(path).exists():
        return None, None, None
    curves = collections.defaultdict(lambda: collections.defaultdict(list))
    # A blank value is a metric that could not be decided -- `compare` failed,
    # so the run has no ideal count -- which is a different fact from a run
    # that found none. Read as zero it drags every mean towards zero silently,
    # so the pair is recorded and excluded from that metric's aggregate.
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
                None if row["censored"] == "" else int(row["censored"]),
            )
        else:
            value = num_or_nan(row["value"])
            if math.isnan(value):
                unknown.add((key, metric))
                curves[key].setdefault(metric, [])
            else:
                curves[key][metric].append((float(row["elapsed_s"]), value))
    # A run that found no ideal repair emits no `ideal_solutions` rows at all
    # when it was also killed, since the terminal point `curve_rows` appends
    # needs the manifest's wall_s and a killed run has none. That is a zero,
    # not an absence. The scalar is what separates the two: its censoring flag
    # is blank exactly where the event could not be decided.
    for key, _ in list(scalars):
        flag = scalars.get((key, "time_to_first_ideal_repair"), (None, 0))[1]
        if flag is None:
            unknown.add((key, "ideal_solutions"))
            unknown.add((key, "maximal_ideal_solutions"))

    for metrics in curves.values():
        for points in metrics.values():
            points.sort()
    return curves, scalars, unknown


def step_at(points, moment):
    """Value of a step function at `moment`, carrying the last point forward.

    Carrying forward past a run's own end is deliberate: both tools stop at
    their own budget, and "found by time t" for a stopped run is what it had
    when it stopped.
    """
    value = 0.0
    for time, count in points:
        if time > moment:
            break
        value = count
    return value


# -- AuRUS reference -----------------------------------------------------------


def load_aurus():
    """Per-run AuRUS facts, and its own solutions(t) series.

    Returns (runs, series) where runs is {(spec, repeat): {...}} and series is
    {(spec, repeat): [(elapsed_s, nsol)]}.
    """
    full = AURUS_DIR / "aurus_full.csv"
    seq = AURUS_DIR / "aurus_series.csv"
    if not full.exists() or not seq.exists():
        return None, None

    runs = {}
    ideal_times = collections.defaultdict(list)
    for row in load_rows(full):
        key = (row["spec"], row["repeat"])
        record = runs.setdefault(key, {
            "status": row["status"], "n_solutions": 0, "n_ideal": 0,
            "final_nsol": num_or_nan(row["final_nsol"]),
        })
        record["n_solutions"] += 1
        if row["hit"] in ("1", "True", "true"):
            record["n_ideal"] += 1
            moment = num_or_nan(row["found_elapsed_s"])
            if not math.isnan(moment):
                ideal_times[key].append(moment)
    for key, record in runs.items():
        times = sorted(ideal_times.get(key, []))
        record["ideal_times"] = times
        record["first_ideal_s"] = times[0] if times else None

    series = collections.defaultdict(list)
    for row in load_rows(seq):
        series[(row["spec"], row["repeat"])].append(
            (num_or_nan(row["elapsed_s"]), num_or_nan(row["nsol"])))
    for points in series.values():
        points.sort()
    return runs, dict(series)


# -- integrity -----------------------------------------------------------------


def integrity(results, curves):
    rule("INTEGRITY")
    print(f"results rows          {len(results)}")
    commits = collections.Counter((r["commit"], r["dirty"]) for r in results)
    for (commit, dirty), n in commits.most_common():
        print(f"  commit {commit} dirty={dirty}: {n} rows")
    if len(commits) > 1:
        print("  WARN: rows span more than one binary")

    specs = sorted({r["spec"] for r in results})
    seeds = sorted({int(r["seed"]) for r in results})
    print(f"families              {len(specs)}")
    print(f"seeds                 {len(seeds)} ({min(seeds)}-{max(seeds)})")

    by_arm = collections.Counter(r["arm"] for r in results)
    for arm in ARMS:
        print(f"  {arm:28s} {by_arm.get(arm, 0)} rows")
    expected = len(specs) * len(seeds)
    if any(by_arm.get(a, 0) != expected for a in ARMS):
        print(f"  WARN: an arm is not the expected {expected} rows")

    duplicates = [k for k, n in collections.Counter(
        (r["arm"], r["key"]) for r in results).items() if n > 1]
    print(f"duplicate (arm, spec, seed) keys: {len(duplicates)}")

    if curves is None:
        print("curves                ABSENT -- curve sections are skipped")
        return
    covered = {(spec, seed, arm) for (spec, seed, arm) in curves}
    wanted = {(r["spec"], int(r["seed"]), r["arm"]) for r in results}
    print(f"curve runs covered    {len(covered)} of {len(wanted)}")
    missing = wanted - covered
    if missing:
        print(f"  WARN: {len(missing)} runs have no curve rows, e.g. "
              f"{sorted(missing)[:3]}")


# -- verification (PLAN section 12) --------------------------------------------


def read_manifests():
    """Every run manifest, plus the directories that hold none."""
    manifests, censored = [], []
    for run_dir in sorted(RUNS_DIR.iterdir()):
        if not run_dir.is_dir():
            continue
        path = run_dir / "run.json"
        if path.exists():
            try:
                manifests.append((run_dir.name, json.loads(path.read_text())))
            except json.JSONDecodeError:
                print(f"  WARN: {run_dir.name}/run.json is not JSON")
        elif (run_dir / "accumulated" / "index.tsv").exists():
            censored.append(run_dir.name)
    return manifests, censored


def verification(results, curves):
    rule("VERIFICATION -- the five checks of PLAN section 12")
    if not RUNS_DIR.exists():
        print("run directories absent; checks 1, 2 and 5 skipped")
        return
    manifests, censored = read_manifests()
    print(f"manifests read {len(manifests)}, censored directories {len(censored)}")

    sub("1. stopped_by must read `individuals`")
    stopped = collections.Counter(m.get("stopped_by") for _, m in manifests)
    for value, n in stopped.most_common():
        print(f"   {str(value):14s} {n}")
    if stopped.get("generations"):
        print("   FAIL: the 500-generation ceiling bound on some runs")
    else:
        print("   PASS: the generation ceiling never bound")

    sub("2. individuals_bred at or just above 1000")
    bred = [m.get("individuals_bred") for _, m in manifests]
    numeric = [b for b in bred if isinstance(b, (int, float))]
    nulls = len(bred) - len(numeric)
    if numeric:
        over = [b - MAX_INDIVIDUALS for b in numeric]
        print(f"   min {min(numeric)}  max {max(numeric)}  "
              f"median {med(numeric)}  null {nulls}")
        print(f"   overshoot: max {max(over)}, runs above cap "
              f"{sum(1 for o in over if o > 0)}")
        print("   PASS" if max(over) <= 0 else "   CHECK: overshoot is non-zero")
    else:
        print(f"   no numeric individuals_bred ({nulls} null)")

    sub("3. ideal_solutions must not be flat zero -- cross-check the join")
    implies = sum(1 for r in results if r["implies_ideal"] == "1")
    print(f"   results CSV implies_ideal: {implies} of {len(results)} "
          f"({pct(implies, len(results))})")
    if curves is not None:
        curve_ideal = sum(
            1 for metrics in curves.values()
            if step_at(metrics.get("ideal_solutions", []), HARNESS_CAP_S) > 0)
        print(f"   curve ideal_solutions > 0: {curve_ideal} of {len(curves)} "
              f"({pct(curve_ideal, len(curves))})")
        if implies and not curve_ideal:
            print("   FAIL: the curve join is broken -- compare's output is not "
                  "reaching the accumulation index")
        else:
            print("   PASS: both sources agree the search finds ideal repairs")
    else:
        print("   curves absent; the cross-check half is skipped")

    sub("4. read cost off this campaign's own rows")
    walls = [num_or_nan(r["wall_time_s"]) for r in results]
    walls = [w for w in walls if not math.isnan(w)]
    if walls:
        print(f"   mean {statistics.mean(walls):.1f}s  median {med(walls)}s  "
              f"max {max(walls):.1f}s  n {len(walls)}")
        print(f"   PLAN section 13 assumed 500s a run; the ratio is "
              f"{statistics.mean(walls) / 500:.2f}x")

    sub("5. count the censored runs first")
    print(f"   censored {len(censored)} of {len(results)} "
          f"({pct(len(censored), len(results))})")
    by_family = collections.Counter(
        name.split("_wkoff_log_")[-1].rsplit("_seed", 1)[0]
        for name in censored)
    for family, n in by_family.most_common(8):
        print(f"     {family:32s} {n}")
    return censored


# -- cost ----------------------------------------------------------------------


def cost(results, censored):
    rule("COST")
    walls = collections.defaultdict(list)
    for row in results:
        value = num_or_nan(row["wall_time_s"])
        if not math.isnan(value):
            walls[row["arm"]].append(value)

    sub("wall time by arm (finished runs only)")
    for arm in ARMS:
        values = walls.get(arm, [])
        if values:
            print(f"   {arm:28s} mean {statistics.mean(values):7.1f}s  "
                  f"median {med(values):>6}s  n {len(values)}")

    # A censored run carries a wall time in the results CSV like any other, so
    # the two shares are a partition of those rows rather than two sums to add.
    done = [num_or_nan(r["wall_time_s"]) for r in results
            if r["timed_out"] != "1"]
    killed = [num_or_nan(r["wall_time_s"]) for r in results
              if r["timed_out"] == "1"]
    done_h = sum(v for v in done if not math.isnan(v)) / 3600
    killed_h = sum(v for v in killed if not math.isnan(v)) / 3600
    total = done_h + killed_h
    sub("where the machine time went")
    print(f"   {len(done)} runs that finished   {done_h:8.1f} core-h  "
          f"({pct(done_h, total)})")
    print(f"   {len(killed)} killed at the cap    {killed_h:8.1f} core-h  "
          f"({pct(killed_h, total)})")
    print(f"   total                     {total:8.1f} core-h")
    print("   A killed run writes no manifest and scores as a failure in every "
          "column, so that share of the machine time bought no scored row.")

    sub("slowest families by mean wall time")
    by_family = collections.defaultdict(list)
    for row in results:
        value = num_or_nan(row["wall_time_s"])
        if not math.isnan(value):
            by_family[row["spec"]].append(value)
    ranked = sorted(by_family.items(),
                    key=lambda kv: -statistics.mean(kv[1]))
    for family, values in ranked[:10]:
        print(f"   {family:32s} mean {statistics.mean(values):7.1f}s  "
              f"n {len(values)}")


# -- curves --------------------------------------------------------------------


def curve_table(curves, metric, unknown):
    """Mean value of `metric` per arm at each shared cut.

    A run whose value for this metric could not be decided is excluded rather
    than counted as zero, and the count of exclusions is printed beside the
    arm so a thin cell is visible rather than merely low.
    """
    sub(f"{metric}: mean per run at each cut")
    # An absent metric means different things on the two halves. A run with no
    # `solutions` or `ideal_solutions` rows found none, and reads zero. A run
    # with no `maximal_*` rows was never put through `maximal`, and is dropped
    # rather than read as having no survivors.
    computed = metric.startswith("maximal")
    scored = {arm: [key for key, metrics in curves.items()
                    if key[2] == arm and (key, metric) not in unknown
                    and (metric in metrics or not computed)]
              for arm in ARMS}
    dropped = {arm: sum(1 for key in curves
                        if key[2] == arm and (key, metric) in unknown)
               for arm in ARMS}
    print("   cut(s)  " + "".join(f"{arm:>22s}" for arm in ARMS))
    print("   runs    " + "".join(f"{len(scored[arm]):>22d}" for arm in ARMS))
    if any(dropped.values()):
        print("   undecided" + "".join(f"{dropped[arm]:>21d}" for arm in ARMS))
    for moment in TIME_CUTS:
        cells = []
        for arm in ARMS:
            values = [step_at(curves[key].get(metric, []), moment)
                      for key in scored[arm]]
            cells.append(f"{statistics.mean(values):22.2f}" if values
                         else f"{'n/a':>22s}")
        print(f"   {moment:6d}  " + "".join(cells))


def curve_sections(curves, unknown):
    if curves is None:
        rule("CURVES -- SKIPPED, no curve CSV")
        return
    rule("CURVES -- counter, by arm")
    print("Mean count per run at each cut, carrying a stopped run's last value")
    print("forward: both tools stop at their own budget, and what a stopped run")
    print("found by time t is what it had when it stopped.")
    for metric in ("solutions", "ideal_solutions",
                   "maximal_solutions", "maximal_ideal_solutions"):
        present = any(metric in m for m in curves.values())
        if present:
            curve_table(curves, metric, unknown)
        else:
            print(f"\n-- {metric}: absent from the curve CSV "
                  f"(was it scored without --maximality?)")


# -- discovery -----------------------------------------------------------------


def survival(times, censored_flags, horizon):
    """Fraction reaching the event by `horizon`, and the median among reachers.

    A median over reachers alone is a biased summary of a censored sample and
    is reported beside the reaching fraction rather than instead of it, which
    is what makes the pair readable.
    """
    reached = [t for t, c in zip(times, censored_flags)
               if c == 0 and not math.isnan(t) and t <= horizon]
    n = sum(1 for c in censored_flags if c is not None)
    return len(reached), n, (statistics.median(reached) if reached else None)


def discovery(curves, scalars, aurus_runs):
    rule("DISCOVERY -- time to first repair, and to first ideal repair")
    if scalars is None:
        print("curve CSV absent; counter's side is skipped")
    else:
        for metric in ("time_to_first_repair", "time_to_first_ideal_repair"):
            sub(f"counter: {metric}")
            for arm in ARMS:
                times, flags = [], []
                for (key, name), (value, flag) in scalars.items():
                    if name != metric or key[2] != arm:
                        continue
                    times.append(value)
                    flags.append(flag)
                reached, n, median = survival(times, flags, HARNESS_CAP_S)
                shown = f"{median:.1f}s" if median is not None else "n/a"
                print(f"   {arm:28s} reached {reached}/{n} "
                      f"({pct(reached, n)})  median among reachers {shown}")

    if aurus_runs is None:
        print("\nAuRUS reference absent; its side is skipped")
        return
    sub("AuRUS: time to first ideal solution")
    usable = {k: v for k, v in aurus_runs.items()
              if v["status"] != "lost-at-cap"}
    lost = len(aurus_runs) - len(usable)
    reached = [v["first_ideal_s"] for v in usable.values()
               if v["first_ideal_s"] is not None]
    print(f"   runs {len(aurus_runs)}, of which {lost} lost-at-cap and carry "
          f"no verdict at all")
    print(f"   reached {len(reached)}/{len(usable)} of the usable runs "
          f"({pct(len(reached), len(usable))})  "
          f"median among reachers {med(reached, 1)}s")
    print("   Resolution is one iteration, dated to the second; counter's is "
          "sub-second. Read the two at the coarser one.")


# -- head to head --------------------------------------------------------------


def head_to_head(results, aurus_runs):
    """Per-family ideal rate, read two ways that bracket the truth.

    A killed run is the whole difficulty. counter's kills score `implies_ideal`
    as 0, so a family whose runs were all killed reads zero by construction.
    AuRUS's kills lose their solution files and carry no verdict, so a family
    whose runs were all killed drops out of the comparison entirely -- which is
    the same event producing opposite biases, and `humanoid-503` (AuRUS killed
    30 of 30) against `humanoid-742` (counter killed 120 of 120) is exactly
    that pair.

    So both readings are printed. `all` scores every run and counts a kill as a
    failure on both sides, which is symmetric and pessimistic. `sc` (scorable)
    drops the kills on both sides, which is symmetric and optimistic. Neither
    is the answer; the accumulator curves are, because they score a killed
    counter run on what it actually found, and nothing on the AuRUS side can
    do the same.
    """
    rule("HEAD TO HEAD -- per-family ideal rate, counter against AuRUS")
    if aurus_runs is None:
        print("AuRUS reference absent; skipped")
        return

    # `no-ideals-dir` is unscorable rather than zero: there was nothing to
    # compare against, so the family carries no AuRUS rate at all.
    aurus_all = collections.defaultdict(list)
    aurus_scorable = collections.defaultdict(list)
    aurus_killed = collections.Counter()
    for (spec, _), record in aurus_runs.items():
        if record["status"] == "no-ideals-dir":
            continue
        hit = 1 if record["n_ideal"] > 0 else 0
        aurus_all[spec].append(hit)
        if record["status"] == "lost-at-cap":
            aurus_killed[spec] += 1
        else:
            aurus_scorable[spec].append(hit)

    counter_all = collections.defaultdict(list)
    counter_scorable = collections.defaultdict(list)
    counter_killed = collections.Counter()
    for row in results:
        hit = 1 if row["implies_ideal"] == "1" else 0
        counter_all[row["spec"]].append(hit)
        if row["timed_out"] == "1":
            counter_killed[row["spec"]] += 1
        else:
            counter_scorable[row["spec"]].append(hit)

    shared = sorted(set(aurus_all) & set(counter_all))
    print(f"families counter covers {len(counter_all)}, "
          f"AuRUS covers {len(aurus_all)}, shared {len(shared)}")
    for label, missing in (("counter only", set(counter_all) - set(aurus_all)),
                           ("AuRUS only", set(aurus_all) - set(counter_all))):
        if missing:
            print(f"  {label}: {', '.join(sorted(missing))}")
    print("AuRUS rates are UNFILTERED -- the well-separation screen's inputs "
          "did not survive on either host.")
    print("`all` counts a killed run as a failure on both sides; `sc` drops "
          "the killed runs on both sides. k is how many were killed.")

    print(f"\n   {'family':30s} {'counter':>8s} {'sc':>7s} {'k':>5s}"
          f"   {'AuRUS':>8s} {'sc':>7s} {'k':>5s}   {'d-all':>7s} {'d-sc':>7s}")
    all_differences, scorable_differences, scorable_families = [], [], []
    for family in shared:
        c_all = statistics.mean(counter_all[family])
        a_all = statistics.mean(aurus_all[family])
        c_sc = counter_scorable.get(family)
        a_sc = aurus_scorable.get(family)
        all_differences.append(c_all - a_all)
        if c_sc and a_sc:
            scorable_differences.append(
                statistics.mean(c_sc) - statistics.mean(a_sc))
            scorable_families.append(family)
        c_cell = f"{statistics.mean(c_sc):7.3f}" if c_sc else f"{'--':>7s}"
        a_cell = f"{statistics.mean(a_sc):7.3f}" if a_sc else f"{'--':>7s}"
        d_cell = (f"{statistics.mean(c_sc) - statistics.mean(a_sc):+7.3f}"
                  if c_sc and a_sc else f"{'--':>7s}")
        print(f"   {family:30s} {c_all:8.3f} {c_cell} "
              f"{counter_killed[family]:5d}   {a_all:8.3f} {a_cell} "
              f"{aurus_killed[family]:5d}   {c_all - a_all:+7.3f} {d_cell}")

    print(f"\n   {POSTHOC}")
    for label, differences, families in (
            ("all runs, a kill is a failure", all_differences, shared),
            ("scorable runs only", scorable_differences, scorable_families)):
        if not differences:
            continue
        higher = sum(1 for d in differences if d > 0)
        lower = sum(1 for d in differences if d < 0)
        print(f"\n   {label}: {len(families)} families")
        print(f"     counter higher on {higher}, lower on {lower}, "
              f"tied on {len(differences) - higher - lower}")
        print(f"     mean difference {statistics.mean(differences):+.3f}, "
              f"exact two-sided Wilcoxon p = "
              f"{wilcoxon_exact_p(differences):.4f}")
    print("\n   The 2026-08-21 ship campaign read this contrast at counter's "
          "own budget as 0.502 against 0.504, Wilcoxon p = 0.7549.")
    print("   This campaign reads it at AuRUS's budget, which is the whole "
          "point of the matched cross.")


# -- post-hoc factor contrasts -------------------------------------------------


def contrast(results, name, left, right, pick):
    """Paired McNemar on found_repair and implies_ideal across one factor."""
    pairs = collections.defaultdict(dict)
    for row in results:
        side = None
        if arm_side(row, left):
            side = "left"
        elif arm_side(row, right):
            side = "right"
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
    faster = sum(1 for v in complete
                 if num_or_nan(v["left"]["wall_time_s"])
                 < num_or_nan(v["right"]["wall_time_s"]))
    means = {side: statistics.mean(
        [num_or_nan(v[side]["wall_time_s"]) for v in complete
         if not math.isnan(num_or_nan(v[side]["wall_time_s"]))])
        for side in ("left", "right")}
    print(f"     wall time        {left} faster on {faster}/{len(complete)}; "
          f"means {means['left']:.1f}s against {means['right']:.1f}s")


def arm_side(row, value):
    return row["selection"] == value or row["level_value"] == value


def posthoc(results):
    rule("POST-HOC FACTOR CONTRASTS -- the 2x2, read after the fact")
    print("PLAN section 2 registers no decision rule and no primary endpoint.")
    print("Nothing below is a test of a hypothesis; it describes this sample.")
    contrast(results, "selection", "nsga2-apportion", "weighted",
             lambda r: r["level_value"])
    contrast(results, "status grading", "mrs", "aurus",
             lambda r: r["selection"])


# -- main ----------------------------------------------------------------------


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", default=RESULTS_CSV)
    parser.add_argument("--curves", default=CURVES_CSV)
    args = parser.parse_args()

    results = load_results(args.results)
    curves, scalars, unknown = load_curves(args.curves)
    aurus_runs, _aurus_series = load_aurus()

    integrity(results, curves)
    censored = verification(results, curves) or []
    cost(results, censored)
    curve_sections(curves, unknown)
    discovery(curves, scalars, aurus_runs)
    head_to_head(results, aurus_runs)
    posthoc(results)

    rule("READING THESE NUMBERS")
    print("Every p-value above is post-hoc; PLAN section 2 registers no rule.")
    print("AuRUS's ideal rates are unfiltered, its curves move only at a")
    print("generation boundary, and its killed runs carry no verdict at all.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
