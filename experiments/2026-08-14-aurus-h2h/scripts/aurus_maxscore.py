#!/usr/bin/env python3
"""Rescore AuRUS h2h output after maximality filtering.

The archived `best_relation` for AuRUS is a maximum over its realizable,
well-separated solutions with no maximality step, while counter's is a maximum
over its maximality-filtered `repair_N` files. This rescores the AuRUS side
under the same filter so the two relation mixes are comparable.

Three scorings per repeat, all against the same ideals in the same run so the
migration between them is paired:
  base  the realizable + well-separated set, reproducing the archive's column
  max   the maximal subset under implication (counter's shipping filter)
  cls   one representative per equivalence class (the pending tie-break)
"""
import argparse, csv, os, re, subprocess, sys, tempfile, time
from pathlib import Path

CLASS_RE = re.compile(r"^class (\d+)\s+(.*\.tlsf)\s*$")


def load_scored(details_csv, host_repeats):
    """{(spec, repeat): [filenames]} for realizable + well-separated files."""
    keep = {}
    with open(details_csv) as fh:
        for row in csv.reader(fh):
            if len(row) < 5:
                continue
            spec, rep, fname, verdict, sep = row[0], row[1], row[2], row[3], row[4]
            if not rep.isdigit() or int(rep) not in host_repeats:
                continue
            if verdict != "REALIZABLE" or sep != "well-separated":
                continue
            keep.setdefault((spec, int(rep)), []).append(fname)
    return keep


def linkdir(tmp, name, repeat_dir, files):
    d = Path(tmp) / name
    d.mkdir()
    for f in files:
        (d / Path(f).name).symlink_to((repeat_dir / Path(f).name).resolve())
    return d


def run_compare(compare_bin, repairs, ideals, timeout, parse):
    try:
        r = subprocess.run([str(compare_bin), "--repairs", str(repairs),
                            "--ideals", str(ideals)],
                           check=True, timeout=timeout,
                           capture_output=True, text=True)
    except (subprocess.TimeoutExpired, subprocess.CalledProcessError):
        return "unknown", 0, 0
    return parse(r.stdout)


def run_maximal(maximal_bin, d, jobs, black_timeout, cap):
    """(maximal_files, class_reps) or (None, None) on timeout."""
    try:
        r = subprocess.run([str(maximal_bin), str(d), "--jobs", str(jobs),
                            "--timeout", str(black_timeout)],
                           timeout=cap, capture_output=True, text=True)
    except subprocess.TimeoutExpired:
        for name in ("ltlfilt", "ltl2tgba", "black"):
            subprocess.run(["pkill", "-9", "-x", name], capture_output=True)
        return None, None
    if r.returncode != 0:
        return None, None
    maximal, reps, seen = [], [], set()
    for line in r.stdout.splitlines():
        m = CLASS_RE.match(line.strip())
        if not m:
            continue
        cid, path = m.group(1), m.group(2)
        maximal.append(path)
        if cid not in seen:
            seen.add(cid)
            reps.append(path)
    return maximal, reps


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--repo", type=Path, required=True)
    p.add_argument("--aurus-out", type=Path, required=True)
    p.add_argument("--details", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--repeats", required=True, help="e.g. 0-4")
    p.add_argument("--jobs", type=int, default=24)
    p.add_argument("--black-timeout", type=int, default=20)
    p.add_argument("--maximal-cap", type=int, default=300)
    p.add_argument("--compare-timeout", type=int, default=600)
    a = p.parse_args()

    sys.path.insert(0, str(a.repo / "scripts"))
    from run_experiments import parse_compare_output as parse

    lo, hi = (int(x) for x in a.repeats.split("-"))
    wanted = set(range(lo, hi + 1))
    scored = load_scored(a.details, wanted)

    cols = ["spec", "repeat", "n_scored", "n_maximal", "n_classes",
            "base_relation", "base_implies", "base_n",
            "max_relation", "max_implies", "max_n",
            "cls_relation", "cls_implies", "cls_n", "elapsed_s", "status"]
    seen = set()
    if a.out.exists():
        with open(a.out) as fh:
            for row in csv.DictReader(fh):
                if row.get("spec"):
                    seen.add((row["spec"], row["repeat"]))
    else:
        with open(a.out, "w") as fh:
            csv.writer(fh).writerow(cols)

    compare_bin = a.repo / "build-release" / "compare"
    maximal_bin = a.repo / "build-release" / "maximal"

    for (spec, rep) in sorted(scored):
        if (spec, str(rep)) in seen:
            continue
        files = sorted(scored[(spec, rep)])
        repeat_dir = a.aurus_out / spec / f"repeat-{rep:02d}"
        ideals = a.repo / "examples" / spec / "fixes"
        row = {"spec": spec, "repeat": rep, "n_scored": len(files),
               "n_maximal": "", "n_classes": "", "status": "OK"}
        t0 = time.time()
        if not repeat_dir.is_dir() or not ideals.is_dir() or len(files) < 2:
            row["status"] = "SKIP"
        else:
            with tempfile.TemporaryDirectory(prefix="maxscore-") as tmp:
                sdir = linkdir(tmp, "scored", repeat_dir, files)
                row["base_relation"], row["base_implies"], row["base_n"] = \
                    run_compare(compare_bin, sdir, ideals, a.compare_timeout, parse)
                mx, reps = run_maximal(maximal_bin, sdir, a.jobs,
                                       a.black_timeout, a.maximal_cap)
                if mx is None:
                    row["status"] = "MAXTIMEOUT"
                else:
                    row["n_maximal"], row["n_classes"] = len(mx), len(reps)
                    mdir = linkdir(tmp, "maximal", repeat_dir, mx)
                    row["max_relation"], row["max_implies"], row["max_n"] = \
                        run_compare(compare_bin, mdir, ideals,
                                    a.compare_timeout, parse)
                    cdir = linkdir(tmp, "classes", repeat_dir, reps)
                    row["cls_relation"], row["cls_implies"], row["cls_n"] = \
                        run_compare(compare_bin, cdir, ideals,
                                    a.compare_timeout, parse)
        row["elapsed_s"] = f"{time.time() - t0:.1f}"
        with open(a.out, "a") as fh:
            csv.DictWriter(fh, cols).writerow(
                {c: row.get(c, "") for c in cols})
        print(f"{spec:28s} r{rep:02d} {row['status']:10s} "
              f"scored={row['n_scored']} max={row['n_maximal']} "
              f"cls={row['n_classes']} "
              f"{row.get('base_relation','')} -> {row.get('max_relation','')} "
              f"[{row['elapsed_s']}s]", flush=True)
    print("MAXSCORE_DONE", flush=True)


if __name__ == "__main__":
    main()
