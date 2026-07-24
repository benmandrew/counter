#!/usr/bin/env python3
"""Run the AuRUS baseline campaign and collect metrics to a results CSV.

The head-to-head arm of the ablation campaign (PLAN §3.3): for each spec x
repeat, invoke AuRUS's scripts/unreal-repair.sh from the AuRUS repo root with
the campaign parameters (-Max=1000 -Gen=1000 -Pop=100 -k=10 -GATO=<gato>
-addA), one output directory per repeat. AuRUS is not seedable (its RNG comes
from Math.random() with no CLI override), so independent repeats stand in for
seeds and every out.txt is archived.

The tool's own unreal-repair-harness.sh is deliberately not used — it is
broken under `set -u` — and per-run wall time is measured here, externally,
rather than trusting the JVM's self-report (recorded too, as aurus_time_s).

Each run's out.txt is parsed (`Num. of Solutions:`, `Time:`, `Settings{...}`)
into a row of <out-root>/aurus_results.csv. Resumable: a (spec, repeat) whose
out.txt already exists is not re-run — its CSV row is backfilled from the
existing out.txt if missing (wall_time_s blank, since the original wall clock
is gone). Repeat-major ordering, so killing the campaign at a wall-clock
deadline leaves a balanced design across specs.

Concurrency is bounded by a pool of worker threads, each owning one JVM
subprocess (--concurrency, default 10 — ~8 GB heap each via the script's
-Xmx8g stays far under the 125 GB av2/av3 machines). A run that outlives
GATO by 300 s is assumed wedged and its whole process group is killed.

Usage:
    python scripts/aurus_campaign.py --out-root ~/aurus-results \\
        --spot-bin ~/projects/counter/build-release/third_party/spot/bin
    python scripts/aurus_campaign.py --out-root out --specs minepump \\
        --repeats 1 --gato 30                      # smoke test
"""

import argparse
import csv
import os
import re
import signal
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

# counter family name -> TLSF path relative to <aurus-root>/case-studies/.
# Keys are the counter examples/ names so the CSV joins directly against
# run_experiments.py's h2h-tlsf rows; values are AuRUS's own layout. This is
# the 12-family head-to-head set (H2H_TLSF_SPECS): the fixes-backed TLSF
# corpus minus amba (no AuRUS case study) and minus counter's own arbiter (a
# different problem from AuRUS's), plus the AuRUS import arbiter-aurus.
# takeoff is excluded on both sides: its upstream "genuine" fixes are both
# invalid, so counter has no ideals to compare against — see EXPERIMENTS.md
# 2026-07-24.
SPEC_TLSF: dict[str, str] = {
    "arbiter-aurus": "arbiter/arbiter.tlsf",
    "codesample-un1":
        "codeSampleV3un1/codeSamples_v3un1simple_Forklift_unrealizable.tlsf",
    "codesample-un2":
        "codeSampleV3un2/codeSamples_v3un2_Forklift_unrealizable.tlsf",
    "gyro-var1":
        "GyroUnrealizable_Var1/"
        "GyroUnrealizable_Var1_710_GyroAspect_unrealizable.tlsf",
    "gyro-var2":
        "GyroUnrealizable_Var2/"
        "GyroUnrealizable_Var2_710_GyroAspect_unrealizable.tlsf",
    "humanoid-458":
        "HumanoidLTL_458/HumanoidLTL_458_Humanoid_fixed_unrealizable.tlsf",
    "humanoid-531":
        "HumanoidLTL_531/HumanoidLTL_531_Humanoid_unrealizable.tlsf",
    "lift": "lift/Lift.tlsf",
    "lily02": "lily02/lilydemo02.tlsf",
    "minepump": "minepump/minepump.tlsf",
    "rg1": "RG1/RG1.tlsf",
    "rg2": "RG2/RG2.tlsf",
}

# Fixed GA parameters of the campaign (PLAN §5); only GATO is a knob, since
# the 3600 s cap is the lever that bounds the phase's worst case.
FIXED_FLAGS = ["-Max=1000", "-Gen=1000", "-Pop=100", "-k=10", "-addA"]

# Grace beyond GATO before the process group is killed: AuRUS's own timeout is
# internal to the GA loop, so a wedged JVM (or a straggling model-counting
# child) can outlive it indefinitely.
KILL_GRACE_S = 300

CSV_FIELDS = [
    "spec", "repeat", "n_solutions", "aurus_time_s", "wall_time_s",
    "killed", "exit_code", "settings",
]

N_SOLUTIONS_RE = re.compile(r"Num\. of Solutions:\s*(\d+)")
# Anchored so "GA Time:" does not match.
TIME_RE = re.compile(r"^Time:\s*(\d+)", re.MULTILINE)
SETTINGS_RE = re.compile(r"Settings\{(.*)\}")


def parse_out_txt(out_txt: Path) -> dict:
    """Return the n_solutions / aurus_time_s / settings columns from out.txt.

    Missing fields stay blank rather than failing the row: an out.txt cut
    short by a kill still records whatever it got to.
    """
    row = {"n_solutions": "", "aurus_time_s": "", "settings": ""}
    try:
        text = out_txt.read_text(errors="replace")
    except OSError:
        return row
    if m := N_SOLUTIONS_RE.search(text):
        row["n_solutions"] = str(int(m.group(1)))
    if m := TIME_RE.search(text):
        row["aurus_time_s"] = str(int(m.group(1)))
    if m := SETTINGS_RE.search(text):
        row["settings"] = m.group(1)
    return row


def run_one(aurus_root: Path, tlsf: Path, out_dir: Path, gato: int,
            env: dict) -> tuple[int | str, int, float]:
    """Execute one AuRUS repair run; return (exit_code, killed, wall_time_s).

    The JVM runs in its own session so a timeout kill takes the whole process
    group (java plus any strix/relsat/ltl2tgba children) rather than just the
    wrapper shell.
    """
    out_dir.mkdir(parents=True, exist_ok=True)
    cmd = [str(aurus_root / "scripts" / "unreal-repair.sh"),
           *FIXED_FLAGS, f"-GATO={gato}", f"-out={out_dir}", str(tlsf)]
    log_path = out_dir / "run.log"
    t_start = time.monotonic()
    killed = 0
    with open(log_path, "wb") as log_file:
        proc = subprocess.Popen(cmd, cwd=aurus_root, env=env,
                                stdout=log_file, stderr=subprocess.STDOUT,
                                start_new_session=True)
        try:
            proc.wait(timeout=gato + KILL_GRACE_S)
        except subprocess.TimeoutExpired:
            killed = 1
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
    wall = round(time.monotonic() - t_start, 2)
    return proc.returncode, killed, wall


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--aurus-root", type=Path,
                        default=Path.home() / "projects" / "tools" / "aurus",
                        metavar="PATH",
                        help="AuRUS checkout (default: %(default)s); must be "
                             "built (bin/ populated by ant compile)")
    parser.add_argument("--out-root", type=Path, required=True, metavar="PATH",
                        help="Directory for <spec>/repeat-NN/ run dirs and "
                             "aurus_results.csv")
    parser.add_argument("--specs", nargs="+", choices=list(SPEC_TLSF),
                        default=list(SPEC_TLSF), metavar="SPEC",
                        help="Specs to run, by counter family name "
                             "(default: the 12-family head-to-head set)")
    parser.add_argument("--repeats", type=int, default=20, metavar="N",
                        help="Independent repeats per spec (default: 20); "
                             "AuRUS is not seedable, so repeats stand in for "
                             "seeds")
    parser.add_argument("--gato", type=int, default=3600, metavar="S",
                        help="AuRUS GA execution timeout in seconds "
                             "(default: 3600); the run is hard-killed at "
                             f"GATO + {KILL_GRACE_S} s if the JVM wedges")
    parser.add_argument("--concurrency", type=int, default=10, metavar="N",
                        help="Concurrent AuRUS runs (default: 10)")
    parser.add_argument("--spot-bin", type=Path, default=None, metavar="PATH",
                        help="Directory prepended to PATH so AuRUS finds "
                             "ltl2tgba/autfilt (e.g. counter's "
                             "build-release/third_party/spot/bin). Omit if "
                             "SPOT is already on PATH")
    args = parser.parse_args()

    if args.concurrency < 1:
        sys.exit("--concurrency must be >= 1")
    repair_sh = args.aurus_root / "scripts" / "unreal-repair.sh"
    if not repair_sh.exists():
        sys.exit(f"Not an AuRUS checkout: {repair_sh} missing")
    if not (args.aurus_root / "bin" / "main" / "Main.class").exists():
        sys.exit(f"AuRUS is not built: run `ant compile` in {args.aurus_root}")

    env = os.environ.copy()
    if args.spot_bin is not None:
        env["PATH"] = f"{args.spot_bin.resolve()}{os.pathsep}{env['PATH']}"

    results_csv = args.out_root / "aurus_results.csv"
    done: set[tuple[str, int]] = set()
    if results_csv.exists():
        with open(results_csv, newline="") as f:
            for row in csv.DictReader(f):
                done.add((row["spec"], int(row["repeat"])))

    # Repeat-major, mirroring run_experiments.py's seed-major order: a
    # wall-clock kill leaves every spec at the same repeat depth.
    tasks = [(spec, rep) for rep in range(args.repeats)
             for spec in args.specs]
    to_run = [(s, r) for s, r in tasks
              if not (args.out_root / s / f"repeat-{r:02d}" / "out.txt").exists()]
    backfill = [(s, r) for s, r in tasks
                if (s, r) not in done and (s, r) not in to_run]

    print("=" * 64)
    print(f"  AuRUS baseline: {len(args.specs)} specs x {args.repeats} repeats")
    print(f"    aurus:       {args.aurus_root}")
    print(f"    out:         {args.out_root}")
    print(f"    GATO:        {args.gato}s (kill at +{KILL_GRACE_S}s)")
    print(f"    concurrency: {args.concurrency}")
    print(f"    plan:        {len(to_run)} to run, {len(backfill)} rows to "
          f"backfill, {len(tasks) - len(to_run) - len(backfill)} already done")
    print("=" * 64)

    args.out_root.mkdir(parents=True, exist_ok=True)
    lock = threading.Lock()
    state = {"completed": 0}
    n_exec = len(to_run)
    t0 = time.monotonic()

    def append_row(row: dict) -> None:
        write_header = (not results_csv.exists()
                        or results_csv.stat().st_size == 0)
        with open(results_csv, "a", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=CSV_FIELDS)
            if write_header:
                writer.writeheader()
            writer.writerow(row)

    # Backfilled rows come from a previous session's out.txt: the run itself
    # is done, only its CSV row is missing (e.g. the session died between the
    # run and the append). Wall time is unrecoverable and stays blank.
    for spec, rep in backfill:
        out_dir = args.out_root / spec / f"repeat-{rep:02d}"
        append_row({"spec": spec, "repeat": rep,
                    **parse_out_txt(out_dir / "out.txt"),
                    "wall_time_s": "", "killed": "", "exit_code": ""})
        done.add((spec, rep))

    def execute(task: tuple[str, int]) -> None:
        spec, rep = task
        run_id = f"{spec}/repeat-{rep:02d}"
        out_dir = args.out_root / spec / f"repeat-{rep:02d}"
        tlsf = args.aurus_root / "case-studies" / SPEC_TLSF[spec]
        with lock:
            print(f"[start]      {run_id}", flush=True)
        exit_code, killed, wall = run_one(
            args.aurus_root, tlsf, out_dir, args.gato, env)
        row = {"spec": spec, "repeat": rep,
               **parse_out_txt(out_dir / "out.txt"),
               "wall_time_s": wall, "killed": killed, "exit_code": exit_code}
        with lock:
            state["completed"] += 1
            n = state["completed"]
            if (spec, rep) not in done:
                append_row(row)
                done.add((spec, rep))
            elapsed = time.monotonic() - t0
            eta = elapsed / n * (n_exec - n)
            note = "  KILLED" if killed else ""
            print(f"[{n}/{n_exec}]  {run_id}  done in {wall}s{note}"
                  f"  ETA {eta/60:.1f}min", flush=True)

    with ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        list(pool.map(execute, to_run))

    elapsed_total = time.monotonic() - t0
    print(f"\nDone. {state['completed']} runs in {elapsed_total/60:.1f} min."
          f"\nResults: {results_csv}")


if __name__ == "__main__":
    main()
