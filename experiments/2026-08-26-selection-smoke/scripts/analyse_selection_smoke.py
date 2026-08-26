#!/usr/bin/env python3
"""Analyse the selection-smoke campaign: `weighted` against `nsga2-apportion`.

Every figure in the campaign's REPORT.md comes from one invocation:

    python3 scripts/analyse_selection_smoke.py experiments/2026-08-26-selection-smoke

Pass `--control <dir>` to run the archived-control validity check, which pairs
this campaign's nsga2-apportion arm against an earlier archive's rows at the
same sweep level and scheme -- 2026-08-23-monotone's `monoon` arm is what it
was written for. The check exists because the two ran the same configuration
on the same corpus and seeds, so a disagreement is evidence of an engine change
or a censoring difference rather than of anything this campaign did.

No third-party dependencies. McNemar is the exact binomial on the discordant
pairs rather than the chi-square approximation, which is wrong at these counts;
the per-family cost test is an exact sign test over the family means, chosen
over Wilcoxon because the wall-time ratios span 1.03x to 3.87x and ranking by
magnitude would let the widest families speak for the corpus.
"""

import argparse
import collections
import csv
import glob
import json
import math
import statistics
from pathlib import Path

CONTROL_ARM = "nsga2-apportion"
TREATMENT_ARM = "weighted"


def exact_binomial_two_sided(b: int, c: int) -> float:
    """Exact two-sided McNemar / sign test on b against c discordant counts."""
    n = b + c
    if n == 0:
        return 1.0
    k = min(b, c)
    tail = sum(math.comb(n, i) for i in range(k + 1)) / 2 ** n
    return min(1.0, 2 * tail)


def truthy(value: str) -> bool:
    return value not in ("", "0", "False", "false")


def ideal(row: dict) -> int:
    return 1 if float(row["implies_ideal"] or 0) > 0 else 0


def found(row: dict) -> int:
    return 1 if float(row["found_repair"] or 0) > 0 else 0


def load_rows(directory: Path, stem: str) -> list:
    path = directory / f"results-{stem}.csv"
    if not path.exists():
        raise SystemExit(f"no merged CSV at {path}")
    return list(csv.DictReader(path.open()))


def pair_up(rows: list) -> dict:
    """(spec, seed) -> {arm: row}, keeping only the complete pairs."""
    by = collections.defaultdict(dict)
    for row in rows:
        by[(row["spec"], row["seed"])][row["selection"]] = row
    return {k: v for k, v in by.items() if len(v) == 2}


def section(title: str) -> None:
    print(f"\n{'=' * 78}\n{title}\n{'=' * 78}")


def report_provenance(rows: list) -> None:
    section("PROVENANCE")
    print(f"rows            {len(rows)}")
    for column in ("commit", "dirty", "level_name", "weakening", "metric",
                   "sweep"):
        print(f"{column:15s} {sorted(set(r[column] for r in rows))}")
    arms = collections.Counter(r["selection"] for r in rows)
    print(f"arms            {dict(arms)}")
    seeds = sorted(set(int(r['seed']) for r in rows))
    print(f"seeds           {min(seeds)}-{max(seeds)} ({len(seeds)} distinct)")
    print(f"families        {len(set(r['spec'] for r in rows))}")


def report_censoring(rows: list, pairs: dict) -> None:
    """Read this before any endpoint.

    The caps were sized from the control arm's archived maxima, it being the
    only arm with any, so they can bite the treatment arm alone -- one-sided
    censoring on the response under test, which is a bias and not a budget.
    """
    section("CENSORING -- read before the endpoints")
    for column in ("timed_out", "compare_timed_out"):
        counts = collections.Counter(
            r["selection"] for r in rows if truthy(r[column]))
        print(f"{column:20s} {dict(counts) or 'none in either arm'}")
    for key, arms in sorted(pairs.items()):
        for arm, row in arms.items():
            if truthy(row["timed_out"]) or truthy(row["compare_timed_out"]):
                other = arms[CONTROL_ARM if arm == TREATMENT_ARM
                             else TREATMENT_ARM]
                print(f"  {key[0]} seed {key[1]}: {arm} capped at "
                      f"{float(row['wall_time_s']):.0f}s, implies_ideal="
                      f"{ideal(row)}; paired arm implies_ideal={ideal(other)}")


def report_endpoints(pairs: dict) -> None:
    section("ENDPOINTS")
    values = list(pairs.values())
    n = len(values)
    print(f"{n} complete pairs\n")
    print(f"{'endpoint':16s} {CONTROL_ARM:>17s} {TREATMENT_ARM:>10s} "
          f"{'ratio':>8s}")
    for name, fn in (("implies_ideal", ideal), ("found_repair", found)):
        a = sum(fn(v[CONTROL_ARM]) for v in values) / n
        b = sum(fn(v[TREATMENT_ARM]) for v in values) / n
        print(f"{name:16s} {a:17.4f} {b:10.4f} {b / a if a else 0:8.3f}")
    for name in ("n_repairs", "wall_time_s", "n_implies", "best_fitness"):
        a = statistics.mean(float(v[CONTROL_ARM][name] or 0) for v in values)
        b = statistics.mean(float(v[TREATMENT_ARM][name] or 0) for v in values)
        print(f"{name:16s} {a:17.2f} {b:10.2f} {b / a if a else 0:8.3f}")

    for name, fn in (("implies_ideal", ideal), ("found_repair", found)):
        both = sum(1 for v in values
                   if fn(v[CONTROL_ARM]) and fn(v[TREATMENT_ARM]))
        neither = sum(1 for v in values
                      if not fn(v[CONTROL_ARM]) and not fn(v[TREATMENT_ARM]))
        b = sum(1 for v in values
                if fn(v[CONTROL_ARM]) and not fn(v[TREATMENT_ARM]))
        c = sum(1 for v in values
                if fn(v[TREATMENT_ARM]) and not fn(v[CONTROL_ARM]))
        print(f"\npaired exact McNemar, {name}:")
        print(f"  both {both}  neither {neither}  "
              f"{CONTROL_ARM}-only {b}  {TREATMENT_ARM}-only {c}")
        print(f"  two-sided p = {exact_binomial_two_sided(b, c):.6f}")


def by_family(pairs: dict) -> dict:
    families = collections.defaultdict(list)
    for (spec, _), arms in pairs.items():
        families[spec].append((arms[CONTROL_ARM], arms[TREATMENT_ARM]))
    return families


def report_families(pairs: dict) -> None:
    section("PER-FAMILY")
    families = by_family(pairs)
    header = (f"{'family':26s} {'n':>3s} {'ctrl':>6s} {'wtd':>6s} "
              f"{'c-only':>6s} {'w-only':>6s} {'wall_c':>8s} {'wall_w':>8s} "
              f"{'x':>5s} {'rep_c':>6s} {'rep_w':>6s}")
    print(header)
    order = sorted(families,
                   key=lambda s: -statistics.mean(
                       float(p[1]["wall_time_s"] or 0) for p in families[s]))
    total_b = total_c = 0
    for spec in order:
        ps = families[spec]
        n = len(ps)
        b = sum(1 for p in ps if ideal(p[0]) and not ideal(p[1]))
        c = sum(1 for p in ps if ideal(p[1]) and not ideal(p[0]))
        total_b, total_c = total_b + b, total_c + c
        wc = statistics.mean(float(p[0]["wall_time_s"] or 0) for p in ps)
        ww = statistics.mean(float(p[1]["wall_time_s"] or 0) for p in ps)
        print(f"{spec:26s} {n:3d} "
              f"{sum(ideal(p[0]) for p in ps) / n:6.2f} "
              f"{sum(ideal(p[1]) for p in ps) / n:6.2f} {b:6d} {c:6d} "
              f"{wc:8.1f} {ww:8.1f} {ww / wc if wc else 0:5.2f} "
              f"{statistics.mean(float(p[0]['n_repairs'] or 0) for p in ps):6.1f} "
              f"{statistics.mean(float(p[1]['n_repairs'] or 0) for p in ps):6.1f}")
    print(f"{'TOTAL':26s} {len(pairs):3d} {'':6s} {'':6s} "
          f"{total_b:6d} {total_c:6d}")

    # Per-family McNemar, Bonferroni-corrected over every family tested rather
    # than over the discordant ones alone: the correction has to count the
    # tests the design licensed, not the ones that happened to be interesting.
    print("\nper-family exact McNemar, families with any discordance "
          f"(Bonferroni over all {len(families)} families):")
    scored = []
    for spec, ps in families.items():
        b = sum(1 for p in ps if ideal(p[0]) and not ideal(p[1]))
        c = sum(1 for p in ps if ideal(p[1]) and not ideal(p[0]))
        if b or c:
            scored.append((exact_binomial_two_sided(b, c), spec, b, c))
    for p, spec, b, c in sorted(scored):
        print(f"  {spec:26s} ctrl-only {b:2d}  wtd-only {c:2d}   "
              f"p={p:.4f}   Bonferroni p={min(1.0, p * len(families)):.3f}")


def report_cost(pairs: dict) -> None:
    section("COST")
    families = by_family(pairs)
    deltas = []
    for ps in families.values():
        a = statistics.mean(float(p[0]["wall_time_s"] or 0) for p in ps)
        b = statistics.mean(float(p[1]["wall_time_s"] or 0) for p in ps)
        deltas.append(b - a)
    slower = sum(1 for d in deltas if d > 0)
    print(f"families where {TREATMENT_ARM} is slower: {slower}/{len(deltas)}")
    print(f"exact sign test two-sided p = "
          f"{exact_binomial_two_sided(len(deltas) - slower, slower):.6f}")
    ratios = []
    for ps in families.values():
        a = statistics.mean(float(p[0]["wall_time_s"] or 0) for p in ps)
        b = statistics.mean(float(p[1]["wall_time_s"] or 0) for p in ps)
        if a:
            ratios.append(b / a)
    print(f"per-family wall ratio: min {min(ratios):.2f}x, "
          f"median {statistics.median(ratios):.2f}x, max {max(ratios):.2f}x")


def report_mechanism(directory: Path, specs: list) -> None:
    """Where the extra wall time goes, from the per-run manifests.

    Only run when the per-run tree is present. It is not archived -- 96 MB of
    it -- so this section is empty in the closed archive and the figures it
    produced are quoted in REPORT.md instead.
    """
    section("MECHANISM (needs the per-run tree, which is not archived)")
    root = directory / "results-selection-smoke"
    if not root.exists():
        print(f"no per-run tree at {root}; see REPORT.md for the recorded "
              f"figures")
        return

    def flatten(mapping, prefix=""):
        out = {}
        for key, value in mapping.items():
            if isinstance(value, dict):
                out.update(flatten(value, prefix + key + "."))
            elif isinstance(value, (int, float)) and not isinstance(value, bool):
                out[prefix + key] = value
        return out

    for spec in specs:
        arms = {}
        for arm in (CONTROL_ARM, TREATMENT_ARM):
            pattern = str(root / f"sweep_T_monoon_{arm}_wkoff_log_{spec}_seed*")
            loaded = []
            for run_dir in sorted(glob.glob(pattern)):
                manifest = Path(run_dir) / "run.json"
                if manifest.exists():
                    loaded.append(flatten(json.load(manifest.open())))
            arms[arm] = loaded
        if not all(arms.values()):
            continue
        print(f"\n{spec} (n={len(arms[CONTROL_ARM])} vs "
              f"{len(arms[TREATMENT_ARM])})")
        keys = [k for k in arms[CONTROL_ARM][0] if k in arms[TREATMENT_ARM][0]]
        for key in keys:
            a = statistics.mean(m.get(key, 0) for m in arms[CONTROL_ARM])
            b = statistics.mean(m.get(key, 0) for m in arms[TREATMENT_ARM])
            if max(a, b) < 1e-9:
                continue
            ratio = b / a if a else float("inf")
            if abs(ratio - 1) > 0.15 or key == "wall_s":
                print(f"  {key:44s} {a:12.2f} {b:12.2f} {ratio:7.2f}")


def report_control_check(pairs: dict, control_dir: Path,
                         control_level: str) -> None:
    """Pair this campaign's control arm against an earlier archive's."""
    section(f"VALIDITY CHECK against {control_dir.name}")
    candidates = list(control_dir.glob("results-*.csv"))
    merged = [p for p in candidates
              if not any(t in p.name for t in ("-av2", "-av3", "manifest"))]
    if not merged:
        print(f"no merged CSV under {control_dir}; skipping")
        return
    archived = [r for r in csv.DictReader(merged[0].open())
                if r["level_name"] == control_level
                and r["selection"] == CONTROL_ARM]
    fresh = {k: v[CONTROL_ARM] for k, v in pairs.items()}
    old = {(r["spec"], r["seed"]): r for r in archived
           if (r["spec"], r["seed"]) in fresh}
    common = sorted(set(old) & set(fresh))
    if not common:
        print("no (spec, seed) overlap; skipping")
        return
    agree = sum(1 for k in common if ideal(old[k]) == ideal(fresh[k]))
    print(f"matched (spec, seed): {len(common)}")
    print(f"implies_ideal: archived "
          f"{sum(ideal(old[k]) for k in common) / len(common):.4f}  fresh "
          f"{sum(ideal(fresh[k]) for k in common) / len(common):.4f}  "
          f"agree on {agree}/{len(common)} ({agree / len(common):.1%})")
    print(f"  archived-only "
          f"{sum(1 for k in common if ideal(old[k]) and not ideal(fresh[k]))}, "
          f"fresh-only "
          f"{sum(1 for k in common if ideal(fresh[k]) and not ideal(old[k]))}")
    for column in ("wall_time_s", "n_repairs"):
        a = statistics.mean(float(old[k][column] or 0) for k in common)
        b = statistics.mean(float(fresh[k][column] or 0) for k in common)
        print(f"  {column:12s} archived {a:8.2f}  fresh {b:8.2f}  "
              f"ratio {b / a if a else 0:.3f}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path,
                        help="the campaign directory holding the merged CSV")
    parser.add_argument("--stem", default="selection-smoke",
                        help="CSV stem (default: selection-smoke)")
    parser.add_argument("--control", type=Path, default=None,
                        help="an archive to run the control-arm validity "
                             "check against, e.g. ../2026-08-23-monotone")
    parser.add_argument("--control-level", default="monoon",
                        help="level_name to select in the control archive "
                             "(default: monoon)")
    parser.add_argument("--mechanism-specs", nargs="*",
                        default=["ltl2dba27", "simple-arbiter-aurus", "rg1"],
                        help="families to break down from the per-run "
                             "manifests, where that tree is present")
    args = parser.parse_args()

    rows = load_rows(args.directory, args.stem)
    pairs = pair_up(rows)

    report_provenance(rows)
    report_censoring(rows, pairs)
    report_endpoints(pairs)
    report_families(pairs)
    report_cost(pairs)
    report_mechanism(args.directory, args.mechanism_specs)
    if args.control:
        control = args.control
        if not control.is_absolute():
            control = (args.directory / control).resolve()
        report_control_check(pairs, control, args.control_level)


if __name__ == "__main__":
    main()
