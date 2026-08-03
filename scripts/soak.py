#!/usr/bin/env python3
"""Long-running soak over the in-process libspot paths, sized for about 24
hours on an idle machine.

Three things the existing coverage does not reach: timeouts that actually fire,
concurrency at campaign scale, and resource behaviour over hours. The
differential suite compares one in-process answer against one tool answer, and
check_engine_parity.py deliberately sets its timeouts far above anything that
could fire. Neither says what happens when one does.

Two arms, one per host:

  control   no timeouts at all, so both in-process calls run inline on the
            calling thread
  deadline  cycles three tiers per round -- tight (5ms/5ms, so essentially
            every call is abandoned and the single libspot lock is never
            free), mid (100ms/50ms, around the knee, where the median call
            finishes and the tail does not), and loose (60s/60s, far above the
            worst call measured, so it must never fire)

Loose against control is the correctness check. Configuring a timeout moves the
work to a worker thread, and that must not change a single repair. Repairs are
hashed with the same digest function as check_engine_parity.py, so a digest
here can be compared with one there.

Tight is the resource check. The failure mode it hunts is not a crash: an
*abandoned* call leaves its worker holding the process-wide libspot lock, so
every later call finds it busy inside the 8ms budget and spawns the tool
instead. The optimisation quietly turns back into the spawning it replaced, and
the only symptom is wall time returning to where it started. The
libspot/simplify-lock-busy and libspot/translate-lock-busy counters are what
make that visible.

Each round runs every example short (10 generations, population 300, 5-minute
cap), then one example long (40 generations, population 1000, 90-minute cap),
rotating which one. Short runs churn processes, which is what would accumulate
orphaned tools across a campaign -- the shape the old ltl2tgba leak took. Long
runs are the only ones that would show heap or thread growth inside a single
process.

Both arms walk the same example list in the same order at the same seeds, so a
soak cut short is still comparable on its prefix. The CSV is append-only and
flushed per run, and re-running resumes past what is already recorded.

Sampled per run: the running process's resident set, thread count and open
descriptors, plus a system-wide census of tool processes whose parent is init.
Peaks go in the per-run CSV and the series in a second one, because whether the
numbers climb or sit flat is the whole question and a peak taken at exit would
miss it.

Usage:
  scripts/soak.py --arm control --out ~/soak-control --hours 24
  scripts/soak.py --arm deadline --out ~/soak-deadline --hours 24
  scripts/soak.py --arm deadline --out /tmp/soak --examples fsm --rounds 1
"""

import argparse
import csv
import hashlib
import json
import os
import shutil
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Every example that carries a spec, in a fixed order so both arms walk the
# same list and a partial run is still comparable on its prefix.
EXAMPLES_DIR = REPO / "examples"

# The long run rotates through these. Chosen because each is known to reach the
# simplification and counting paths heavily rather than failing early.
LONG_ROTATION = ["fsm", "lift", "takeoff", "arbiter", "amba"]

BASE_SEED = 20260803

# Shared by every configuration written here. The TLSF caps are not optional:
# ltlsynt is multi-GB resident per call on the harder specs, and without them a
# 24-hour soak over 22 TLSF examples ends as an OOM kill rather than a result.
COMMON_RUNTIME = {
    "parallel": "8",
    "black_timeout_ms": "1000",
    "ltlsynt_timeout_ms": "10000",
    "max_concurrent_realizability": "4",
    # A translate deadline that fires drops the individual, and the default
    # 0.05 aborts the run a second or two in -- which is the correct campaign
    # behaviour and useless here, because a soak that stops before it has
    # loaded anything soaks nothing. Set for both arms rather than the tight
    # tier alone, so the arms differ in the timeout keys and nothing else.
    "max_scoring_failure_rate": "0.95",
}

# name -> (ltl2tgba_timeout_ms, simplify_timeout_ms)
TIERS = {
    "control": {"none": (0, 0)},
    "deadline": {
        # Below anything either path can finish in, so essentially every call
        # is abandoned and the lock is never free.
        "tight": (5, 5),
        # Around the knee: the median call finishes, the tail does not.
        "mid": (100, 50),
        # Far above the worst call measured, so it must never fire. This is the
        # arm whose repairs have to equal control's, byte for byte.
        "loose": (60000, 60000),
    },
}

# (generations, population, per-run wall cap in seconds)
SHORT_SHAPE = (10, 300, 300)
LONG_SHAPE = (40, 1000, 5400)

# Counters read back out of each run's profile JSON. Absent means zero: a
# counter is only registered once something increments it.
PROFILE_COUNTERS = [
    "libspot/simplify-timed-out",
    "libspot/simplify-lock-busy",
    "libspot/translate-timed-out",
    "libspot/translate-lock-busy",
]

# Scope call counts, which is where the fallback-spawn rate comes from.
PROFILE_SCOPES = [
    "ltlfilt/one-shot-exec",
    "ltlfilt/batch-exec",
    "ltlfilt/libspot-simplify",
    "ltlfilt/libspot-simplify-worker",
    "spot/libspot-translate",
    "spot/libspot-translate-worker",
    "proc/fork+exec",
]


def column_for(scope: str, suffix: str) -> str:
    return scope.replace("/", ".") + "." + suffix


RUN_FIELDS = (
    ["started_at", "round", "kind", "tier", "example", "seed", "generations",
     "population", "exit_code", "timed_out", "wall_s", "n_repairs", "digest",
     "peak_rss_kb", "peak_threads", "peak_fds", "orphans_after", "profile_ok",
     "unexpected"]
    + [c.split("/")[-1] for c in PROFILE_COUNTERS]
    + [column_for(s, k) for s in PROFILE_SCOPES
       for k in ("calls", "max_wall_ms")]
)

SAMPLE_FIELDS = ["at", "round", "kind", "tier", "example", "elapsed_s",
                 "rss_kb", "threads", "fds", "orphans"]

# Tools the run spawns. Any of these left with init as a parent is a leak; the
# translation path has produced exactly that before.
ORPHAN_NAMES = {"ltl2tgba", "ltlfilt", "ltlsynt", "black", "ganak", "autfilt"}


def examples() -> list[str]:
    found = []
    for path in sorted(EXAMPLES_DIR.iterdir()):
        if not path.is_dir():
            continue
        if (path / "spec.json").exists() or (path / "spec.tlsf").exists():
            found.append(path.name)
    return found


def spec_for(example: str) -> Path:
    for name in ("spec.json", "spec.tlsf"):
        candidate = EXAMPLES_DIR / example / name
        if candidate.exists():
            return candidate
    raise FileNotFoundError(f"no spec for {example}")


def write_config(path: Path, generations: int, population: int,
                 ltl2tgba_ms: int, simplify_ms: int) -> None:
    runtime = dict(COMMON_RUNTIME)
    # Written even when zero. Zero is the documented "no timeout" value, so
    # stating it keeps the configuration a full record of the arm rather than
    # something that has to be read against the defaults.
    runtime["ltl2tgba_timeout_ms"] = str(ltl2tgba_ms)
    runtime["simplify_timeout_ms"] = str(simplify_ms)
    lines = ["[genetic]", f"generations = {generations}",
             f"population_size = {population}", "", "[runtime]"]
    lines += [f"{key} = {value}" for key, value in sorted(runtime.items())]
    path.write_text("\n".join(lines) + "\n")


def digest(output_dir: Path) -> str:
    """One hash over every repair, in a fixed order.

    Named by index rather than content, so sorting by name is sorting by rank
    and a reordering of equally-scored repairs is a difference rather than
    being normalised away. Deliberately the same function as
    check_engine_parity.py's, so a digest here can be compared with one there.
    """
    sha = hashlib.sha256()
    for repair in sorted(output_dir.glob("repair_*.json")):
        sha.update(repair.name.encode())
        sha.update(repair.read_bytes())
    return sha.hexdigest()[:16]


def count_orphans() -> int:
    """Tool processes whose parent is init.

    A counter that exits leaves none behind. Any that persist have outlived the
    run that spawned them, which is the shape the old ltl2tgba leak took: not a
    crash, just resident memory that nothing is going to free.
    """
    try:
        out = subprocess.run(["ps", "-eo", "comm=,ppid="], capture_output=True,
                             text=True, timeout=30).stdout
    except (subprocess.SubprocessError, OSError):
        return -1
    total = 0
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] in ORPHAN_NAMES and parts[1] == "1":
            total += 1
    return total


class Sampler(threading.Thread):
    """Polls one process's resident set, thread count and descriptors.

    A peak taken at exit would miss the shape entirely: what matters is whether
    the numbers climb across a long run or sit flat, so the series is written
    as well as the maximum.
    """

    # A quarter second because the shortest runs finish in about one, and a
    # sampler that starts after the process has gone records a peak of zero.
    # It is still possible to miss one; the long runs are what the series is
    # for, and they cannot be missed.
    def __init__(self, pid: int, writer, flush, tags: dict, interval=0.25,
                 series_every=5.0):
        super().__init__(daemon=True)
        self.pid = pid
        self.writer = writer
        self.flush = flush
        self.tags = tags
        self.interval = interval
        self.series_every = series_every
        self.stop_event = threading.Event()
        self.peak_rss = 0
        self.peak_threads = 0
        self.peak_fds = 0

    def _read(self):
        rss = threads = 0
        try:
            with open(f"/proc/{self.pid}/status") as handle:
                for line in handle:
                    if line.startswith("VmRSS:"):
                        rss = int(line.split()[1])
                    elif line.startswith("Threads:"):
                        threads = int(line.split()[1])
        except OSError:
            return None
        try:
            fds = len(os.listdir(f"/proc/{self.pid}/fd"))
        except OSError:
            fds = 0
        return rss, threads, fds

    def run(self):
        start = time.time()
        last_series = 0.0
        while not self.stop_event.is_set():
            reading = self._read()
            if reading is None:
                break
            rss, threads, fds = reading
            self.peak_rss = max(self.peak_rss, rss)
            self.peak_threads = max(self.peak_threads, threads)
            self.peak_fds = max(self.peak_fds, fds)
            elapsed = time.time() - start
            if elapsed - last_series >= self.series_every:
                last_series = elapsed
                row = dict(self.tags)
                row.update({"at": int(time.time()), "elapsed_s": round(elapsed, 1),
                            "rss_kb": rss, "threads": threads, "fds": fds,
                            "orphans": count_orphans()})
                self.writer.writerow(row)
                self.flush()
            self.stop_event.wait(self.interval)


def read_profile(path: Path) -> dict:
    """The run's profile JSON, or an empty report if it cannot be read.

    Tolerant rather than fatal, and recorded either way. A soak that dies on
    its 200th run because one report came back malformed has thrown away the
    199 it already had -- but a malformed report is also a finding in its own
    right (one was: the counter registry was printing freed memory), so
    profile_ok carries it into the CSV instead of hiding it.
    """
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(errors="replace"))
    except (json.JSONDecodeError, OSError, ValueError):
        return {"__unreadable__": True}


def profile_columns(report: dict) -> dict:
    counters = report.get("counters") or {}
    sites = {site.get("name"): site for site in report.get("sites") or []}
    row = {c.split("/")[-1]: counters.get(c, 0) for c in PROFILE_COUNTERS}
    # Anything the report named that this script does not expect. Usually it
    # means a counter was added and this list was not updated; it has also
    # meant the names themselves were corrupt, which is worth seeing rather
    # than silently reading as "every counter was zero".
    row["unexpected"] = ";".join(sorted(set(counters) - set(PROFILE_COUNTERS)))
    row["profile_ok"] = int(not report.get("__unreadable__", False)
                            and bool(report))
    for scope in PROFILE_SCOPES:
        entry = sites.get(scope) or {}
        row[column_for(scope, "calls")] = entry.get("calls", 0)
        # The worst single call, not the mean. A deadline shows up as a ceiling
        # on the caller's worst and nowhere else -- the mean barely moves,
        # because the calls that hit it are the tail by definition.
        row[column_for(scope, "max_wall_ms")] = round(
            entry.get("max_wall_ns", 0) / 1e6, 3)
    return row


def run_one(binary: Path, example: str, seed: int, config: Path, cap: int,
            work: Path, sample_writer, sample_flush, tags: dict) -> dict:
    out = work / "out"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    profile_path = work / "profile.json"
    if profile_path.exists():
        profile_path.unlink()

    env = dict(os.environ)
    env["COUNTER_PROFILE"] = str(profile_path)

    started = time.time()
    # Its own process group, so the cap kills the tools it spawned as well as
    # the run itself. Killing only the parent is what leaves the orphans this
    # is trying to count.
    process = subprocess.Popen(
        [str(binary), "--input", str(spec_for(example)),
         "--output-dir", str(out), "--config", str(config),
         "--seed", str(seed)],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, env=env,
        start_new_session=True)

    sampler = Sampler(process.pid, sample_writer, sample_flush, tags)
    sampler.start()
    timed_out = False
    try:
        process.communicate(timeout=cap)
    except subprocess.TimeoutExpired:
        timed_out = True
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except OSError:
            pass
        process.communicate()
    finally:
        sampler.stop_event.set()
        sampler.join(timeout=10)
    wall = time.time() - started

    row = {
        "started_at": int(started),
        "exit_code": process.returncode,
        "timed_out": int(timed_out),
        "wall_s": round(wall, 2),
        "n_repairs": len(list(out.glob("repair_*.json"))),
        "digest": digest(out),
        "peak_rss_kb": sampler.peak_rss,
        "peak_threads": sampler.peak_threads,
        "peak_fds": sampler.peak_fds,
        "orphans_after": count_orphans(),
    }
    row.update(profile_columns(read_profile(profile_path)))
    return row


def jobs_for_round(index: int, arm: str, names: list[str]):
    """One round: every example short, then one long.

    Short first, so a round that runs out of time still contributes its whole
    corpus pass rather than most of one long run.
    """
    seed = BASE_SEED + index
    tiers = TIERS[arm]
    for tier, (ltl2tgba_ms, simplify_ms) in tiers.items():
        for example in names:
            yield {"kind": "short", "tier": tier, "example": example,
                   "seed": seed, "shape": SHORT_SHAPE,
                   "timeouts": (ltl2tgba_ms, simplify_ms)}
    long_example = LONG_ROTATION[index % len(LONG_ROTATION)]
    for tier, (ltl2tgba_ms, simplify_ms) in tiers.items():
        yield {"kind": "long", "tier": tier, "example": long_example,
               "seed": seed, "shape": LONG_SHAPE,
               "timeouts": (ltl2tgba_ms, simplify_ms)}


def completed_keys(path: Path) -> set:
    if not path.exists():
        return set()
    done = set()
    with path.open() as handle:
        for row in csv.DictReader(handle):
            done.add((row["round"], row["kind"], row["tier"], row["example"],
                      row["seed"]))
    return done


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arm", choices=sorted(TIERS), required=True)
    parser.add_argument("--binary", default="build-release/counter")
    parser.add_argument("--out", required=True)
    parser.add_argument("--hours", type=float, default=24.0)
    parser.add_argument("--rounds", type=int, default=1000)
    parser.add_argument("--examples", nargs="*",
                        help="narrow the corpus; default is every example "
                             "carrying a spec")
    args = parser.parse_args()

    binary = Path(args.binary)
    if not binary.is_absolute():
        binary = REPO / binary
    if not binary.exists():
        print(f"no binary at {binary}", file=sys.stderr)
        return 2

    out_root = Path(args.out).expanduser()
    out_root.mkdir(parents=True, exist_ok=True)
    work = out_root / "work"
    work.mkdir(exist_ok=True)
    config_dir = out_root / "configs"
    config_dir.mkdir(exist_ok=True)

    version = subprocess.run([str(binary), "--version"], capture_output=True,
                             text=True).stdout
    (out_root / "manifest.json").write_text(json.dumps({
        "arm": args.arm,
        "host": os.uname().nodename,
        "started_at": int(time.time()),
        "hours": args.hours,
        "binary": str(binary),
        "version": version,
        "tiers": {name: list(value) for name, value in TIERS[args.arm].items()},
        "short_shape": list(SHORT_SHAPE),
        "long_shape": list(LONG_SHAPE),
        "common_runtime": COMMON_RUNTIME,
    }, indent=2) + "\n")

    runs_path = out_root / "runs.csv"
    samples_path = out_root / "samples.csv"
    done = completed_keys(runs_path)
    if done:
        print(f"resuming: {len(done)} runs already recorded")

    names = args.examples or examples()
    print(f"{len(names)} examples, arm {args.arm}, {args.hours}h budget")

    deadline = time.time() + args.hours * 3600
    runs_new = not runs_path.exists()
    samples_new = not samples_path.exists()
    with runs_path.open("a", newline="") as runs_file, \
            samples_path.open("a", newline="") as samples_file:
        run_writer = csv.DictWriter(runs_file, fieldnames=RUN_FIELDS)
        sample_writer = csv.DictWriter(samples_file, fieldnames=SAMPLE_FIELDS)
        if runs_new:
            run_writer.writeheader()
        if samples_new:
            sample_writer.writeheader()
        runs_file.flush()
        samples_file.flush()

        for index in range(args.rounds):
            if time.time() >= deadline:
                break
            for job in jobs_for_round(index, args.arm, names):
                if time.time() >= deadline:
                    break
                key = (str(index), job["kind"], job["tier"], job["example"],
                       str(job["seed"]))
                if key in done:
                    continue
                generations, population, cap = job["shape"]
                # Never let one run outlive the budget: a long run started with
                # ten minutes left would otherwise push the finish 90 minutes
                # past it.
                cap = int(min(cap, max(30, deadline - time.time())))
                config = config_dir / (
                    f"{job['tier']}-{job['kind']}.toml")
                write_config(config, generations, population, *job["timeouts"])
                tags = {"round": index, "kind": job["kind"],
                        "tier": job["tier"], "example": job["example"]}
                row = run_one(binary, job["example"], job["seed"], config, cap,
                              work, sample_writer, samples_file.flush, tags)
                row.update({"round": index, "kind": job["kind"],
                            "tier": job["tier"], "example": job["example"],
                            "seed": job["seed"], "generations": generations,
                            "population": population})
                run_writer.writerow(row)
                runs_file.flush()
                print(f"r{index} {job['tier']:8} {job['kind']:5} "
                      f"{job['example']:22} exit={row['exit_code']:3} "
                      f"{row['wall_s']:8.1f}s  {row['n_repairs']:3} repairs  "
                      f"{row['digest']}  rss={row['peak_rss_kb']//1024}M "
                      f"thr={row['peak_threads']} orph={row['orphans_after']}",
                      flush=True)

    print("soak finished")
    return 0


if __name__ == "__main__":
    sys.exit(main())
