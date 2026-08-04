#!/usr/bin/env python3
"""Reads a soak's two arms and answers the four questions it was run to settle.

  1. correctness  the loose tier configures deadlines far above anything that
                  can fire, so its repairs must equal the control arm's. Any
                  disagreement is the in-process path answering differently.
  2. resources    peak resident set per run, by tier. A firing deadline is the
                  only thing bounding it, because translation that used to sit
                  in a killable child now runs in counter's own address space.
  3. drift        the same figures bucketed by hour. A leak shows as a rising
                  floor, an accumulation of abandoned workers as a rising
                  thread count.
  4. reversion    an abandoned call leaves its worker holding the libspot lock,
                  so later calls find it busy and spawn the tool instead. The
                  lock-busy counters are the only visible symptom.

Wall-clock comparisons between the two arms are not load-bearing: they ran on
different hosts. Only digests and per-run resource figures are.
"""

import argparse
import collections
import csv
import statistics
from pathlib import Path

# A run killed at the wall cap dies on SIGKILL, which skips the atexit report,
# so it writes no repairs and hashes to the digest of an empty set. Comparing
# that against anything says only that both runs were killed.
EMPTY_DIGEST = "e3b0c44298fc1c14"

INT_FIELDS = ("round", "seed", "generations", "population", "exit_code",
              "n_repairs", "peak_rss_kb", "peak_threads", "peak_fds",
              "orphans_after", "started_at")

COUNTERS = ("translate-timed-out", "translate-lock-busy",
            "simplify-timed-out", "simplify-lock-busy")


def load(path: Path, arm: str) -> list[dict]:
    with path.open() as handle:
        rows = list(csv.DictReader(handle))
    for row in rows:
        for field in INT_FIELDS:
            row[field] = int(row[field]) if row.get(field) else 0
        row["wall_s"] = float(row["wall_s"])
        row["timed_out"] = row["timed_out"].lower() in ("1", "true", "yes")
        row["arm"] = arm
    return rows


def count(row: dict, field: str) -> int:
    value = row.get(field) or ""
    try:
        return int(float(value))
    except ValueError:
        return 0


def key(row: dict) -> tuple:
    return (row["kind"], row["example"], row["seed"], row["generations"],
            row["population"])


def percentiles(values: list[float]) -> tuple[float, float, float, float]:
    ordered = sorted(values)
    last = len(ordered) - 1
    at = lambda q: ordered[min(last, int(q * len(ordered)))]  # noqa: E731
    return at(0.5), at(0.9), at(0.99), ordered[-1]


def compare(control: list[dict], deadline: list[dict], tier: str) -> dict:
    """Digest agreement for one tier against control, ignoring capped runs."""
    index: dict = {}
    for row in control:
        index.setdefault(key(row), []).append(row)
    result = {"same": 0, "differ": 0, "skipped_capped": 0, "unmatched": 0,
              "mismatches": []}
    for row in deadline:
        if row["tier"] != tier:
            continue
        peers = index.get(key(row))
        if not peers:
            result["unmatched"] += 1
        elif row["timed_out"] or peers[0]["timed_out"]:
            result["skipped_capped"] += 1
        elif row["digest"] == peers[0]["digest"]:
            result["same"] += 1
        else:
            result["differ"] += 1
            result["mismatches"].append((row, peers[0]))
    return result


def section(title: str) -> None:
    print()
    print("=" * 74)
    print(title)
    print("=" * 74)


def report_shape(arms: dict) -> None:
    section("1. CAMPAIGN SHAPE")
    for name, rows in arms.items():
        span = (max(r["started_at"] for r in rows) -
                min(r["started_at"] for r in rows)) / 3600
        capped = sum(1 for r in rows if r["timed_out"])
        failed = sum(1 for r in rows if r["exit_code"] and not r["timed_out"])
        print(f"{name:10s} {len(rows):5d} runs over {span:5.1f}h  "
              f"kinds={dict(collections.Counter(r['kind'] for r in rows))}  "
              f"tiers={dict(collections.Counter(r['tier'] for r in rows))}")
        print(f"{'':10s} capped at the wall limit: {capped:4d}   "
              f"non-zero exit that was not the cap: {failed}")


def report_correctness(control: list[dict], deadline: list[dict]) -> None:
    section("2. CORRECTNESS: loose (deadlines that must never fire) vs control")
    result = compare(control, deadline, "loose")
    total = result["same"] + result["differ"]
    print(f"comparable pairs: {total}    agree: {result['same']}    "
          f"disagree: {result['differ']}")
    print(f"skipped because one side was capped: {result['skipped_capped']}   "
          f"no control counterpart: {result['unmatched']}")
    for row, peer in result["mismatches"]:
        print(f"  MISMATCH {row['kind']:5s} {row['example']:20s} "
              f"seed={row['seed']}  loose={row['digest']}"
              f"({row['n_repairs']}r, {row['wall_s']:.0f}s)  "
              f"control={peer['digest']}({peer['n_repairs']}r, "
              f"{peer['wall_s']:.0f}s)  "
              f"fired: translate={count(row, 'translate-timed-out')} "
              f"simplify={count(row, 'simplify-timed-out')}")

    print()
    print("firing deadlines in the tier that should have none:")
    fired = [r for r in deadline if r["tier"] == "loose" and
             (count(r, "translate-timed-out") or
              count(r, "simplify-timed-out"))]
    for row in fired:
        print(f"  {row['kind']:5s} {row['example']:20s} seed={row['seed']}  "
              f"translate={count(row, 'translate-timed-out')} "
              f"simplify={count(row, 'simplify-timed-out')}")
    if not fired:
        print("  none")


def report_firing(control: list[dict], deadline: list[dict],
                  tiers: list[str]) -> None:
    section("3. TIERS THAT DO FIRE: how often does the answer change?")
    for tier in tiers:
        result = compare(control, deadline, tier)
        total = result["same"] + result["differ"]
        fewer = sum(1 for a, b in result["mismatches"]
                    if a["n_repairs"] < b["n_repairs"])
        more = sum(1 for a, b in result["mismatches"]
                   if a["n_repairs"] > b["n_repairs"])
        share = 100 * result["differ"] / total if total else 0
        print(f"{tier:6s} comparable={total:4d}  same={result['same']:4d}  "
              f"differ={result['differ']:4d} ({share:3.0f}%)  "
              f"fewer repairs={fewer:4d}  more={more:4d}")


def report_resources(arms: dict) -> None:
    section("4. PEAK RESIDENT SET PER RUN (MB)")
    for kind in sorted({r["kind"] for rows in arms.values() for r in rows}):
        print(f"-- {kind} runs --")
        for name, rows in arms.items():
            for tier in sorted({r["tier"] for r in rows}):
                selected = [r for r in rows
                            if r["kind"] == kind and r["tier"] == tier]
                if not selected:
                    continue
                med, p90, p99, top = percentiles(
                    [r["peak_rss_kb"] / 1024 for r in selected])
                print(f"  {name+'/'+tier:22s} n={len(selected):4d}  "
                      f"median={med:8.0f}  p90={p90:8.0f}  p99={p99:8.0f}  "
                      f"max={top:9.0f}")
    print()
    print("worst single runs:")
    everything = [r for rows in arms.values() for r in rows]
    for row in sorted(everything, key=lambda r: -r["peak_rss_kb"])[:8]:
        print(f"  {row['peak_rss_kb']/1024:8.0f} MB  {row['arm']:8s} "
              f"{row['tier']:6s} {row['kind']:5s} {row['example']:20s} "
              f"wall={row['wall_s']:7.1f}s capped={row['timed_out']} "
              f"repairs={row['n_repairs']}")


def report_drift(arms: dict, hours: int = 6) -> None:
    section(f"5. DRIFT OVER THE RUN ({hours}-hour buckets)")
    for name, rows in arms.items():
        ordered = sorted(rows, key=lambda r: r["started_at"])
        start = ordered[0]["started_at"]
        buckets: dict = collections.defaultdict(list)
        for row in ordered:
            buckets[(row["started_at"] - start) // (hours * 3600)].append(row)
        print(f"-- {name} --")
        for index in sorted(buckets):
            group = buckets[index]
            rss = [r["peak_rss_kb"] / 1024 for r in group]
            print(f"  h{index*hours:02d}-{index*hours+hours:02d}  "
                  f"n={len(group):4d}  rss median={statistics.median(rss):7.0f}"
                  f"  max={max(rss):8.0f}  "
                  f"threads max={max(r['peak_threads'] for r in group):3d}  "
                  f"fds max={max(r['peak_fds'] for r in group):4d}  "
                  f"orphans={sum(r['orphans_after'] for r in group)}")


def report_reversion(arms: dict) -> None:
    section("6. LOCK CONTENTION AND FIRING DEADLINES")
    for name, rows in arms.items():
        for tier in sorted({r["tier"] for r in rows}):
            selected = [r for r in rows if r["tier"] == tier]
            totals = {c: sum(count(r, c) for r in selected) for c in COUNTERS}
            print(f"  {name+'/'+tier:22s} n={len(selected):4d}  " +
                  "  ".join(f"{c}={totals[c]}" for c in COUNTERS))
    print()
    print("share of libspot work that stayed in process:")
    for name, rows in arms.items():
        for tier in sorted({r["tier"] for r in rows}):
            selected = [r for r in rows if r["tier"] == tier and
                        r["profile_ok"].lower() in ("1", "true")]
            if not selected:
                continue
            line = []
            for label, calls, busy in (
                    ("translate", "spot.libspot-translate.calls",
                     "translate-lock-busy"),
                    ("simplify", "ltlfilt.libspot-simplify.calls",
                     "simplify-lock-busy")):
                inline = sum(count(r, calls) for r in selected)
                spawned = sum(count(r, busy) for r in selected)
                total = inline + spawned
                line.append(f"{label}={100*inline/total:.1f}%" if total
                            else f"{label}=n/a")
            print(f"  {name+'/'+tier:22s} " + "  ".join(line))


def report_throughput(arms: dict) -> None:
    section("7. THROUGHPUT")
    for kind in sorted({r["kind"] for rows in arms.values() for r in rows}):
        for name, rows in arms.items():
            for tier in sorted({r["tier"] for r in rows}):
                selected = [r for r in rows
                            if r["kind"] == kind and r["tier"] == tier]
                if not selected:
                    continue
                finished = [r for r in selected if not r["timed_out"]]
                wall = sum(r["wall_s"] for r in selected)
                repairs = sum(r["n_repairs"] for r in selected)
                wasted = sum(r["wall_s"] for r in selected if r["timed_out"])
                print(f"{kind:5s} {name+'/'+tier:22s} n={len(selected):4d}  "
                      f"finished={100*len(finished)/len(selected):3.0f}%  "
                      f"wall={wall/3600:6.2f}h  repairs={repairs:5d}  "
                      f"per run={repairs/len(selected):4.2f}  "
                      f"per wall-hour={repairs/(wall/3600):7.1f}  "
                      f"wasted on capped runs={wasted/3600:5.2f}h")


def report_health(arms: dict) -> None:
    section("8. HARNESS HEALTH")
    for name, rows in arms.items():
        unexpected = [r for r in rows if r["unexpected"] and
                      r["unexpected"].lower() not in ("", "none", "false", "0")]
        unreadable = [r for r in rows
                      if r["profile_ok"].lower() in ("0", "false")
                      and not r["timed_out"]]
        empty = sum(1 for r in rows
                    if r["digest"] == EMPTY_DIGEST and not r["timed_out"])
        print(f"  {name:10s} unexpected={len(unexpected)}  "
              f"unreadable profile on a run that finished={len(unreadable)}  "
              f"finished with no repairs={empty}")
        for row in unexpected[:6]:
            print(f"    {row['tier']:6s} {row['example']:20s} "
                  f"exit={row['exit_code']} -> {row['unexpected']}")


def main() -> int:
    parser = argparse.ArgumentParser(description="analyse a soak")
    parser.add_argument("--control", type=Path, required=True,
                        help="directory holding the control arm's runs.csv")
    parser.add_argument("--deadline", type=Path, required=True,
                        help="directory holding the deadline arm's runs.csv")
    args = parser.parse_args()

    control = load(args.control / "runs.csv", "control")
    deadline = load(args.deadline / "runs.csv", "deadline")
    arms = {"control": control, "deadline": deadline}

    report_shape(arms)
    report_correctness(control, deadline)
    report_firing(control, deadline,
                  [t for t in sorted({r["tier"] for r in deadline})
                   if t != "loose"])
    report_resources(arms)
    report_drift(arms)
    report_reversion(arms)
    report_throughput(arms)
    report_health(arms)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
