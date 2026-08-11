#!/usr/bin/env python3
"""Tests for campaign.py: parsing, process detection, collect, stage and queue.

No pytest dependency, matching test_experiment_paths.py: run it directly
(``python scripts/test_campaign.py``) and it exits non-zero on the first
failure. Nothing here touches a lab machine — the remote protocol is exercised
against captured marker output, and ``collect`` is exercised against two
throwaway checkouts under a temporary directory, so the merge, the natural key
and the verification all run for real without a 29GB transfer.

The failure modes worth guarding are the silent ones. Process detection keyed
on the whole command line would match this tool's own ssh command and report a
finished campaign as running; a plan line read positionally would start
reporting the aliasing count as the row total; and a collect that loses rows or
duplicates keys looks exactly like a collect that worked. Two hosts declaring
overlapping seed ranges is the same shape of failure one level up: both run the
overlap, the merge keeps one row per key, and the campaign costs more and
yields less than it says while every table reads normal.

The stage and queue tests run against throwaway git checkouts under a temporary
directory, with COUNTER_RUNNER_CMD pointed at a stub that records its arguments
instead of running a campaign. No lab machine is touched and no run is launched.
"""

import argparse
import contextlib
import csv
import importlib
import io
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import campaign as C  # noqa: E402
import merge_experiments as merge  # noqa: E402

# The script under test, as a path rather than through the module's __file__,
# which is typed as optional and is not a path the moment a module is built
# some other way.
CAMPAIGN_PY = Path(__file__).resolve().parent / "campaign.py"


FAILURES: list = []


def fail(msg):
    """Record a failure and carry on to the rest of the suite.

    Exiting on the first one hid whatever came after it, which is how an
    ambient-state dependency in the stage fixtures went unnoticed on a busy
    machine: 79 of the 227 assertions here sat behind the one that failed.
    """
    print(f"FAIL: {msg}")
    FAILURES.append(msg)


def check(got, want, msg):
    if got != want:
        fail(f"{msg}\n  got:  {got!r}\n  want: {want!r}")


def check_true(cond, msg):
    if not cond:
        fail(msg)


# ── The runner's Plan line ────────────────────────────────────────────────────

PLAN = "400 result rows (0 via aliasing), 400 already done; 0 runs to execute"

check(C.parse_plan_line(PLAN),
      {"rows_planned": 400, "rows_done_plan": 400, "runs_left": 0},
      "plan line as run_experiments.py prints it")

check(C.parse_plan_line(
    "7600 result rows (1520 via aliasing), 42 already done; 7558 runs to execute"),
    {"rows_planned": 7600, "rows_done_plan": 42, "runs_left": 7558},
    "aliasing count must not be read as the row total")

# Each number is read from the phrase naming it, so a clause inserted ahead of
# one does not reassign the others.
check(C.parse_plan_line(
    "Plan: 12 skipped, 100 result rows (0 via aliasing), 5 already done; "
    "95 runs to execute")["rows_planned"],
    100, "an added leading clause must not shift rows_planned")

check(C.parse_plan_line("nothing resembling a plan"), {},
      "a non-plan line yields no counts rather than a wrong one")


# ── Inventory and detail parsing ──────────────────────────────────────────────

MANIFEST = """{
  "profile": "arbiter-probe",
  "hostname": "av3",
  "started": "2026-08-11T12:53:14+0100",
  "git": {"branch": "feat/arbiter-probe", "describe": "x",
          "head": "b093374f0a0d8e83d0a9fe3412e6935bffc5b078"},
  "binaries": {"counter": {"commit_short": "b093374", "dirty": "0"}},
  "sweep": {"sweeps": ["R"], "specs": ["amba"], "seeds": [20, 21], "jobs": 1,
            "results_csv": "experiments/results-arbiter-probe.csv",
            "results_dir": "experiments/results-arbiter-probe"}
}"""

INVENTORY = f"""##PS
zsh -zsh
python3 python3 scripts/run_experiments.py --profile arbiter-probe --jobs 1
##HOST
av3
1786460000
##GIT
feat/arbiter-probe
b093374
0
##MANIFESTS
##FILE experiments/results-arbiter-probe-manifest-av3.json
{MANIFEST}
##ENDFILE
##END
"""

inv = C.parse_inventory(INVENTORY)
check(inv["hostname"], "av3", "hostname section")
check(inv["epoch"], 1786460000, "host clock sampled in the same call")
check(inv["branch"], "feat/arbiter-probe", "checkout branch")
check(inv["head"], "b093374", "checkout head")
check(inv["dirty"], False, "clean checkout")
check(len(inv["manifests"]), 1, "one manifest parsed")
check(inv["error"], None, "no error on a good inventory")

check(C.parse_inventory("##ERR not a directory: /nope")["error"],
      "not a directory: /nope", "a missing checkout reports rather than raises")

# A manifest that is not valid JSON is skipped, not fatal: a launch killed
# mid-write leaves one, and it must not take the whole poll down.
broken = C.parse_inventory("##MANIFESTS\n##FILE x\n{not json\n##ENDFILE\n##END\n")
check(broken["manifests"], [], "an unparseable manifest is skipped")

campaigns = C.campaigns_from_manifests(inv["manifests"])
check(len(campaigns), 1, "one campaign per manifest")
c = campaigns[0]
check(c["profile"], "arbiter-probe", "profile from the manifest")
check(c["branch"], "feat/arbiter-probe", "launch branch from the manifest")
check(c["binary_commit"], "b093374", "binary commit from the manifest")
check(c["results_csv"], "experiments/results-arbiter-probe.csv", "csv path")

check(C.plan_args(c),
      "--profile arbiter-probe --dry-run --sweeps R --specs amba --seeds 20 21",
      "plan args replay the manifest's own selection")

no_sweeps = dict(c, sweeps=None)
check_true("--sweeps" not in C.plan_args(no_sweeps),
           "a manifest recording every sweep must not pin one")

DETAIL = """##CAMPAIGN arbiter-probe
##CSVROWS 401
##CSVMTIME 1786455660
##LOGMTIME 1786455673.4823
##RUNDIRS 400
##PLAN 400 result rows (0 via aliasing), 400 already done; 0 runs to execute
##CAMPAIGN empty-one
##CSVROWS 1
##END
"""
detail = C.parse_detail(DETAIL)
check(detail["arbiter-probe"]["rows_done"], 400, "done rows from the plan")
check(detail["arbiter-probe"]["rows_planned"], 400, "planned rows from the plan")
check(detail["arbiter-probe"]["csv_rows"], 400, "wc -l counts the header too")
check(detail["arbiter-probe"]["log_mtime"], 1786455673, "fractional mtime")
check_true("rows_from_csv" not in detail["arbiter-probe"],
           "a campaign with a plan is not on the fallback")
check(detail["empty-one"]["rows_done"], 0, "a header-only CSV has zero rows")
check_true("rows_planned" not in detail["empty-one"],
           "a profile the checkout no longer defines reports no plan")
check(detail["empty-one"]["rows_from_csv"], True,
      "and falls back to the file's length, marked as such")

# The CSV's length is not this launch's progress. Five of the profiles in
# merge_experiments.PROFILE_CSVS share three files, and a top-up relaunch
# rewrites the manifest to a couple of seeds while the file keeps every earlier
# row. Reading the file's length as done would report 400/40 here and, worse,
# call a campaign that started minutes ago finished.
TOPUP = """##CAMPAIGN replicate
##CSVROWS 401
##PLAN 40 result rows (0 via aliasing), 0 already done; 40 runs to execute
##END
"""
topup = C.parse_detail(TOPUP)["replicate"]
check(topup["rows_done"], 0, "a top-up reads its own share, not the whole file")
check(topup["rows_planned"], 40, "against its own plan")
shared = dict(topup, profile="replicate", branch="b", started=None)
C.annotate(shared, {"epoch": 1786460000, "processes": []})
check(shared["state"], "stalled",
      "a just-relaunched campaign must not be reported done")

# Profiles really do share CSVs, which is what makes the whole-file count wrong
# rather than merely imprecise: results-ablate-tlsf.csv is written by two
# profiles and results-replicate.csv by three.
names = list(merge.PROFILE_CSVS.values())
shared = sorted({n for n in names if names.count(n) > 1})
check_true(shared, "PROFILE_CSVS still has profiles sharing a CSV")
check_true(sum(names.count(n) for n in shared) >= 4,
           "and several profiles on each, so the file's length is not one "
           "campaign's progress")


# ── Process detection ─────────────────────────────────────────────────────────

# The last line is this tool's own ssh command as the remote ps sees it: its
# text names run_experiments.py, and matching on the command line rather than
# on comm would report an idle machine as running.
PS = [
    "zsh -zsh",
    "sshd sshd: benandrew@notty",
    "zsh zsh -c cd /home/benandrew/projects/counter && python3 "
    "scripts/run_experiments.py --profile full --dry-run",
    "python3 python3 scripts/run_experiments.py --profile tlsf --jobs 4",
    "counter /home/benandrew/projects/counter/build-release/counter --input x",
]
procs = C.live_processes(PS)
check([p["comm"] for p in procs], ["python3", "counter"],
      "only the runner and the engine, never the shell that names them")
check(procs[0]["profile"], "tlsf", "the runner's profile is read off its args")

check(C.live_processes([
    "python3 python3 scripts/run_experiments.py --profile full --dry-run"]),
    [], "a --dry-run runner is a status probe, not a campaign")

check(C.profile_of_args("python3 scripts/run_experiments.py --jobs 4"), "full",
      "an unflagged runner is on the default profile")
check(C.profile_of_args("python3 x.py --profile=muc"), "muc", "--profile=VALUE")


# ── The remote scripts must survive the lab's login shell ─────────────────────

# The lab shell is zsh, whose default NOMATCH aborts the script where a glob
# matches nothing — so on a host with no top-level manifest a globbed sweep
# takes every later section with it, silently. Run the real inventory script
# under zsh against a checkout that has none and require the closing marker.
empty = Path(tempfile.mkdtemp(prefix="campaign-noglob-"))
try:
    (empty / "experiments").mkdir()
    (empty / ".git").mkdir()
    for shell in ("sh", "zsh"):
        if shutil.which(shell) is None:
            continue
        proc = subprocess.run([shell, "-c", C.inventory_script(str(empty))],
                              capture_output=True, text=True, timeout=60)
        check_true("##END" in proc.stdout,
                   f"the inventory script must reach ##END under {shell} "
                   f"with no manifest present")
        check_true("##MANIFESTS" in proc.stdout,
                   f"and still emit every section under {shell}")
finally:
    shutil.rmtree(empty, ignore_errors=True)


# ── A timed-out probe keeps what arrived ──────────────────────────────────────

# Sections stream out in order, so a probe cut short still holds every record
# that got its turn. Discarding them flips a finished campaign to "stalled"
# purely because the poll was slow.
slow = "printf '##CAMPAIGN p\\n##CSVROWS 401\\n'; sleep 30"
partial, error = C.run_shell(C.LOCAL, slow, timeout=2)
check_true(C.is_timeout(error), "the slow probe did time out")
check(C.parse_detail(partial or "")["p"]["csv_rows"], 400,
      "and its completed records are still parsed")


# ── Derived state ─────────────────────────────────────────────────────────────

def annotated(rows_done, rows_planned, log_mtime, procs, epoch=1786460000,
              started="2026-08-11T12:53:14+0100"):
    campaign = {"profile": "p", "branch": "b", "started": started,
                "rows_done": rows_done, "rows_planned": rows_planned,
                "log_mtime": log_mtime}
    C.annotate(campaign, {"epoch": epoch, "processes": procs})
    return campaign


done = annotated(400, 400, 1786455673, [])
check(done["state"], "done", "complete and idle reads as done")
check(round(done["pct"]), 100, "percent complete")
check(done["eta_s"], None, "a finished campaign has no ETA")

stalled = annotated(797, 800, 1786455673, [])
check(stalled["state"], "stalled", "incomplete with no live process is stalled")
check(int(stalled["pct"]), 99, "797/800 must not read as 100%")

running = annotated(200, 400, 1786459000,
                    [{"comm": "python3", "profile": "p"}])
check(running["state"], "running", "a runner on this profile means running")
check_true(running["eta_s"] is not None, "a running campaign gets an ETA")
# Half the rows in the elapsed time implies roughly that much again.
check(running["eta_s"], running["elapsed_s"], "ETA extrapolates the row rate")

# Staleness is differenced against the host's own clock, sampled in the same
# call, because av3 runs minutes away from av2 whenever NTP is off.
check(annotated(1, 2, 1786459940, [])["stale_s"], 60,
      "staleness uses the host clock, not this machine's")

other = annotated(1, 2, 1786459940, [{"comm": "python3", "profile": "other"}])
check(other["state"], "stalled", "a runner on another profile is not this one")

# An unattributable process must match no campaign. One unrelated `counter` on
# av2 otherwise reported all six of its archived campaigns as running, while
# idle av3 reported the same six from the same data as stalled -- the same
# campaign in two states, decided by a process belonging to neither.
engine = annotated(1, 2, 1786459940, [{"comm": "counter", "profile": None}])
check(engine["state"], "stalled",
      "a process that names no profile must claim no campaign")
check(engine["running"], False, "and must not read as running")

both_hosts = [annotated(1, 2, 1786459940, procs) for procs in
              ([], [{"comm": "counter", "profile": None}])]
check(both_hosts[0]["state"], both_hosts[1]["state"],
      "an idle host and a host busy on other work must agree about the same "
      "archived campaign")

# A matched process is not evidence on its own. Nine days without a run.log is
# far past any single run's timeout, and "running" beside that is the reading
# that stops someone investigating a dead run.
nine_days = 1786460000 - 9 * 86400
stuck = annotated(1, 2, nine_days, [{"comm": "python3", "profile": "p"}])
check(stuck["state"], "stuck",
      "a live runner over a long-dead log must not read as running")
check(stuck["eta_s"], None, "and must not offer an ETA")
check_true(stuck["running"], "though the process is still recorded as matched")

host = {"host": "av2", "reachable": True, "hostname": "av2", "branch": "b",
        "head": "abc1234", "dirty": False, "hidden": 0,
        "processes": [{"comm": "python3", "profile": "p"}],
        "campaigns": [dict(stuck, profile="p", branch="b",
                           binary_commit="abc1234")]}
check_true(any("producing nothing" in n for n in C.status_notes([host])),
           "and says so distinctly rather than collapsing to stalled")

# The boundary is the harness's own bound on one run's silence, so a campaign
# inside it is still running on the same evidence.
fresh = annotated(1, 2, 1786460000 - C.STALE_RUN_S + 60,
                  [{"comm": "python3", "profile": "p"}])
check(fresh["state"], "running", "a log inside the threshold still corroborates")
just_past = annotated(1, 2, 1786460000 - C.STALE_RUN_S - 60,
                      [{"comm": "python3", "profile": "p"}])
check(just_past["state"], "stuck", "and one outside it does not")

# No log at all is a campaign whose first run directory does not exist yet.
no_log = annotated(0, 400, None, [{"comm": "python3", "profile": "p"}])
check(no_log["state"], "running",
      "a just-launched campaign with no run.log yet is not stuck")

check(C.human_duration(None), "-", "unknown duration")
check(C.human_duration(45), "45s", "seconds")
check(C.human_duration(3900), "1h05m", "hours and minutes")
check(C.human_duration(300000), "3d11h", "days and hours")
check(C.human_bytes(0), "0B", "zero bytes")
check(C.human_bytes(3774873), "3.6MB", "megabytes")


# ── The status table ──────────────────────────────────────────────────────────

unreachable = {"host": "av2", "reachable": False,
               "error": "ssh: connect to host av2 port 22: No route to host",
               "campaigns": [], "processes": []}
rows = C.status_rows([unreachable])
check(len(rows), 1, "an unreachable host costs one row, not the run")
check_true(rows[0][1].startswith("(unreachable: ssh: connect"),
           "and says why")
check_true(len(rows[0][1]) <= 60,
           "a paragraph-long ssh error must not widen the whole column")

timed_out = dict(unreachable, error="timed out after 45s")
check_true(C.status_rows([timed_out])[0][1].startswith("(no answer:"),
           "a deadline is reported as a deadline, not as a dead host")
check(C.is_timeout("timed out after 45s"), True, "timeout recognised")
check(C.is_timeout("ssh: no route to host"), False, "an ssh failure is not one")
check(C.is_timeout(None), False, "no error is not a timeout")

fallback = {"host": "av2", "reachable": True, "hostname": "av2",
            "branch": "b", "head": "abc1234", "dirty": False, "hidden": 0,
            "processes": [],
            "campaigns": [dict(topup, rows_done=401, rows_planned=None,
                               rows_from_csv=True, profile="replicate",
                               branch="b", binary_commit="abc1234",
                               state="stalled", pct=None)]}
check(C.status_rows([fallback])[0][2], "~401/?",
      "a whole-CSV count is marked in the table, never passed off as progress")
check_true(any("whole CSV" in n for n in C.status_notes([fallback])),
           "and explained in a note")

check(C.checkout_rows([unreachable])[0][5], "unreachable",
      "the checkout table degrades too")

live = {"host": "av3", "reachable": True, "hostname": "av3",
        "branch": "feat/x", "head": "abc1234", "dirty": True, "hidden": 0,
        "processes": [{"comm": "counter", "profile": None},
                      {"comm": "counter", "profile": None}],
        "campaigns": [dict(done, branch="feat/other", binary_commit="b093374",
                           dirty_binary=True)]}
check(C.checkout_rows([live])[0],
      ["av3", "av3", "feat/x", "abc1234", "dirty", "counter"],
      "the checkout table names the branch a resumed run would use")
check(C.status_rows([live])[0][7], "feat/other!",
      "a campaign launched on a branch the checkout has left is flagged")
check(C.status_rows([live])[0][8], "b093374*",
      "a campaign launched off a dirty binary is flagged")
check_true(any("dirty" in n for n in C.status_notes([live])),
           "and the flag is explained in a note")


# ── collect: merge and verification against local fixtures ────────────────────

CSV_HEADER = ["sweep", "level_name", "selection", "weakening", "metric",
              "repair_mode", "spec", "seed", "commit", "dirty", "wall_time_s"]


def write_csv(path: Path, rows: list[dict]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=CSV_HEADER)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def row(seed, spec="amba", selection="nsga2-truncate", commit="aaaaaaa"):
    return {"sweep": "R", "level_name": "elit0.1", "selection": selection,
            "weakening": "wkon", "metric": "direct", "repair_mode": "mono",
            "spec": spec, "seed": seed, "commit": commit, "dirty": "0",
            "wall_time_s": "1.0"}


def make_checkout(root: Path, rows: list[dict], seeds: list[int]) -> None:
    write_csv(root / "experiments" / "results-fixture.csv", rows)
    for seed in seeds:
        run = root / "experiments" / "results-fixture" / f"run_seed{seed:02d}"
        run.mkdir(parents=True, exist_ok=True)
        (run / "run.log").write_text(f"seed {seed}\n")


def collect_args(**kwargs) -> argparse.Namespace:
    base = {"profile": "fixture", "host": None, "dry_run": False,
            "no_results": False, "csv": "results-fixture.csv",
            "results_dir": "results-fixture"}
    base.update(kwargs)
    return argparse.Namespace(**base)


tmp = Path(tempfile.mkdtemp(prefix="campaign-test-"))
real_merge_root = merge.REPO_ROOT
try:
    hosts = {"h1": tmp / "h1", "h2": tmp / "h2"}
    make_checkout(hosts["h1"], [row(s) for s in (0, 1)], [0, 1])
    make_checkout(hosts["h2"], [row(s) for s in (2, 3)], [2, 3])
    dest = tmp / "dest"
    (dest / "experiments").mkdir(parents=True)

    C.REPO_ROOT = dest
    C.HOSTS = ("h1", "h2")
    C.source_root = lambda host: str(hosts[host])
    # pull_source resolves the per-run destination against merge's own repo
    # root, which is the real checkout. Both have to point at the fixture or
    # the test writes result trees into the working tree.
    merge.REPO_ROOT = dest

    merged = dest / "experiments" / "results-fixture.csv"
    check(C.cmd_collect(collect_args()), 0, "a clean two-host collect verifies")
    check(len(merge.read_rows(merged)[1]), 4, "both halves land in the merge")
    check_true((dest / "experiments" / "results-fixture" /
                "run_seed03" / "run.log").exists(),
               "the per-run trees come across too")

    # Idempotent: re-collecting an already merged campaign overlaps on every
    # key, which the union arithmetic must read as "already here" rather than
    # as a duplicate.
    check(C.cmd_collect(collect_args()), 0, "re-collecting still verifies")
    check(len(merge.read_rows(merged)[1]), 4, "and adds no rows")

    # A host whose CSV records the same runs under a different binary must not
    # split the key: `commit` is not a key field, and a collect that made it
    # one would double every row.
    check_true("commit" not in merge.KEY_FIELDS,
               "commit must never join the natural key")
    write_csv(hosts["h2"] / "experiments" / "results-fixture.csv",
              [row(s, commit="bbbbbbb") for s in (2, 3)])
    check(C.cmd_collect(collect_args()), 0, "a rebuilt binary re-collects clean")
    check(len(merge.read_rows(merged)[1]), 4, "and still yields four rows")

    # The old selection spelling names the same cell, so it must join rather
    # than merge into a second row.
    write_csv(hosts["h2"] / "experiments" / "results-fixture.csv",
              [row(s, selection="nsga2") for s in (2, 3)])
    check(C.cmd_collect(collect_args()), 0, "a retired scheme name re-collects")
    check(len(merge.read_rows(merged)[1]), 4, "and does not split the key")

    # A host asked for that contributes nothing is the silent-loss case the
    # union check cannot see: computed over the survivors alone it agrees with
    # itself perfectly and would print OK over half a campaign. Both ways a
    # host can drop out are covered -- a transfer that raises, and one that
    # returns no CSV.
    for label, sabotage in (
        ("rsync fails", lambda: shutil.rmtree(hosts["h2"])),
        ("no CSV", lambda: (hosts["h2"] / "experiments"
                            / "results-fixture.csv").unlink()),
    ):
        make_checkout(hosts["h2"], [row(s) for s in (2, 3)], [2, 3])
        sabotage()
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            code = C.cmd_collect(collect_args())
        printed = buffer.getvalue()
        check(code, 1, f"a collect losing a host must exit non-zero ({label})")
        check_true("OK: every source row" not in printed,
                   f"and must not print the OK line ({label})")
        check_true("INCOMPLETE" in printed and "h2" in printed,
                   f"and must name the host it lost ({label})")

    # What did arrive is still merged, because the merge is keyed and
    # idempotent: a later run against the recovered host completes the file.
    check(len(merge.read_rows(merged)[1]), 4, "the surviving rows are kept")

    make_checkout(hosts["h2"], [row(s) for s in (2, 3)], [2, 3])
    check(C.cmd_collect(collect_args()), 0,
          "and the recovered host collects clean afterwards")

    # Verification has to be able to fail: a merged file holding one key twice
    # is exactly what a broken merge produces, and it must say so.
    doubled = dest / "experiments" / "doubled.csv"
    write_csv(doubled, [row(0), row(0)])
    check(C.verify_merge(doubled, set(), {}), False,
          "a duplicated key must fail verification")

    lossy = dest / "experiments" / "lossy.csv"
    write_csv(lossy, [row(0)])
    check(C.verify_merge(lossy, {merge.key_of(row(9))}, {}), False,
          "a row present before the merge and absent after must fail")
finally:
    merge.REPO_ROOT = real_merge_root
    shutil.rmtree(tmp, ignore_errors=True)

# ── TOML, in the subset the campaign files use ────────────────────────────────

# campaign.py parses TOML itself because tick runs on av2 and av3, whose
# python3 is 3.10 with no tomllib and no tomli. The risk that buys is a parser
# that quietly disagrees with every other reader of the same file, so each
# fixture below is checked against tomllib wherever tomllib exists.
importlib.reload(C)

DECLARATION = """
# The campaign this file declares.
name = "arbiter-probe"
branch = "feat/arbiter-probe"
profile = "arbiter-probe"
hosts = { av2 = "0-99", av3 = "100-199" }
phases = [ { profile = "arbiter-probe", jobs = 4 } ]
"""

HEADER_FORM = """
name = "two-phase"
branch = "feat/two-phase"
hosts = { av2 = "0-9" }

[[phases]]
name = "warm"
profile = "tlsf"
jobs = 2
specs = ["amba", "lily02"]

[[phases]]
profile = "muc"
jobs = 4
"""

try:
    import tomllib
except ImportError:  # 3.10, which is what the lab machines run.
    tomllib = None

for label, text in (("inline form", DECLARATION), ("header form", HEADER_FORM),
                    ("an entry", C.dump_toml(
                        {"campaign": "x", "state": "queued", "phase": 0,
                         "log": ['a "quoted" line', "b"], "done": True}))):
    parsed = C.parse_toml(text)
    if tomllib is not None:
        check(parsed, tomllib.loads(text),
              f"the built-in parser must agree with tomllib on {label}")

check(C.parse_toml(DECLARATION)["hosts"], {"av2": "0-99", "av3": "100-199"},
      "an inline table of host seed ranges")
check(C.parse_toml(HEADER_FORM)["phases"][0]["specs"], ["amba", "lily02"],
      "[[phases]] headers are the same thing as the inline array")

roundtrip = {"campaign": "x", "seeds": "0-99", "phase": 2, "state": "failed",
             "log": ['said "no"', "line\ttwo"], "flag": False}
check(C.parse_toml(C.dump_toml(roundtrip)), roundtrip,
      "an entry survives being written and read back")

for bad, why in (
    ('a = { b = 1,\n c = 2 }', "a newline inside an inline table"),
    ("a = 1.5", "a float, which nothing here uses"),
    ('a = 1\na = 2', "a duplicate key"),
    ('a = "unterminated', "an unterminated string"),
    ("[t]\n[t]", "a duplicate table"),
):
    try:
        C.parse_toml(bad)
        check_true(False, f"{why} must be rejected")
    except C.TomlError:
        pass


# ── The campaign declaration ──────────────────────────────────────────────────

def write_declaration(root: Path, name: str, text: str) -> Path:
    path = root / "experiments" / name / "campaign.toml"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    return path


def declaration_error(root: Path, name: str, text: str) -> str:
    write_declaration(root, name, text)
    try:
        C.load_campaign(name, root)
    except C.CampaignError as exc:
        return str(exc)
    return ""


decl_root = Path(tempfile.mkdtemp(prefix="campaign-decl-"))
try:
    # The same shape as DECLARATION above, on a profile this checkout defines:
    # arbiter-probe's own profile lives on its branch, which is the normal case
    # and is why load_campaign checks against the runner rather than a list.
    write_declaration(decl_root, "probe", DECLARATION
                      .replace("arbiter-probe", "probe")
                      .replace('profile = "probe"', 'profile = "tlsf"'))
    campaign = C.load_campaign("probe", decl_root)
    check(campaign["branch"], "feat/probe", "branch from the file")
    check(campaign["hosts"]["av2"], list(range(100)), "a range expands")
    check(len(campaign["hosts"]["av3"]), 100, "and so does the other host's")
    check(campaign["phases"][0]["profile"], "tlsf", "the phase profile")
    check(campaign["build"], C.DEFAULT_BUILD_CMD, "the build command defaults")

    # The error worth the most noise. Overlapping ranges cost twice the machine
    # time and yield one row per key, so the campaign is smaller and slower
    # than it says while every table reads normal.
    overlap = declaration_error(decl_root, "overlap", """
name = "overlap"
branch = "feat/x"
hosts = { av2 = "0-99", av3 = "90-199" }
phases = [ { profile = "full", jobs = 1 } ]
""")
    check_true("share 10 seed(s)" in overlap and "av2" in overlap
               and "av3" in overlap,
               f"overlapping seed ranges must be refused by name: {overlap!r}")
    check_true("90" in overlap, "and must name a shared seed")

    for text, expect_in, why in (
        ('hosts = { av2 = "0-" }', "not a seed", "an open-ended range"),
        ('hosts = { av2 = "9-0" }', "backwards", "a range counting backwards"),
        ('hosts = { av2 = "amba" }', "not a seed", "a non-numeric range"),
        ('hosts = { av2 = "" }', "non-empty", "an empty range"),
        ('hosts = { av2 = "0-2,1-3" }', "repeats", "a range repeating itself"),
        ('hosts = { nowhere = "0-1" }', "unknown host", "an unknown host"),
    ):
        got = declaration_error(decl_root, "malformed", f"""
name = "malformed"
branch = "feat/x"
{text}
phases = [ {{ profile = "full", jobs = 1 }} ]
""")
        check_true(expect_in in got, f"{why} must be refused ({got!r})")

    unknown = declaration_error(decl_root, "unknown-profile", """
name = "unknown-profile"
branch = "feat/x"
hosts = { av2 = "0-1" }
phases = [ { profile = "no-such-profile", jobs = 1 } ]
""")
    check_true("defines no profile" in unknown,
               "a profile the runner does not define must be refused")
    check_true("full" in unknown, "and the known ones listed")

    # campaign.toml names a profile; it never redefines one. Eighteen archived
    # campaigns vendor their own copy of run_experiments.py, so a second copy
    # of that table here would be the nineteenth to keep in step.
    check(C.known_profiles(), set(__import__("run_experiments").PROFILES),
          "the profile list comes from the runner, not from a copy")

    for text, expect_in, why in (
        ('name = "wrong"\nbranch = "b"\nhosts = { av2 = "0" }\n'
         'phases = [ { profile = "full" } ]', "directory is",
         "a name that disagrees with its directory"),
        ('name = "decl"\nbranch = "b"\nhosts = { av2 = "0" }\n'
         'phases = []', "non-empty array", "no phases"),
        ('name = "decl"\nbranch = "b"\nhosts = { }\n'
         'phases = [ { profile = "full" } ]', "non-empty table", "no hosts"),
        ('name = "decl"\nbranch = "b"\nseeds = "0-9"\nhosts = { av2 = "0" }\n'
         'phases = [ { profile = "full" } ]', "unknown key",
         "a key nobody reads"),
        ('name = "decl"\nbranch = "b"\nhosts = { av2 = "0" }\n'
         'phases = [ { profile = "full", jobs = 0 } ]', "positive integer",
         "a job count of zero"),
    ):
        got = declaration_error(decl_root, "decl", text)
        check_true(expect_in in got, f"{why} must be refused ({got!r})")

    check_true("name = None" in declaration_error(decl_root, "absent", ""),
               "an empty file is not a valid declaration")
    missing = ""
    try:
        C.load_campaign("never-declared", decl_root)
    except C.CampaignError as exc:
        missing = str(exc)
    check_true("no campaign declaration" in missing,
               "and a campaign with no declaration at all names the path")
finally:
    shutil.rmtree(decl_root, ignore_errors=True)

check(C.parse_seed_range("0-9,20", "x"), list(range(10)) + [20],
      "a comma-separated range")
check(C.format_seed_range([0, 1, 2, 5, 9, 10]), "0-2,5,9-10",
      "and the inverse collapses runs")
check(C.format_seed_range(C.parse_seed_range("100-199", "x")), "100-199",
      "a range survives the round trip a queue entry puts it through")

phase = {"name": "p", "profile": "tlsf", "jobs": 4, "sweeps": ["R"],
         "specs": None}
check(C.phase_args(phase, [0, 1, 2]),
      ["--profile", "tlsf", "--jobs", "4", "--sweeps", "R",
       "--seeds", "0", "1", "2"],
      "a phase becomes runner arguments, seeds last")


# ── stage: the refusals ───────────────────────────────────────────────────────

PROBE = """##GIT
feat/arbiter-probe
b093374f0a0d8e83d0a9fe3412e6935bffc5b078
##DIRTY
 M src/main.cpp
##PS
zsh -zsh
counter /home/benandrew/projects/counter/build-release/counter --input x
##BIN
commit=b093374f0a0d8e83d0a9fe3412e6935bffc5b078
commit_short=b093374
dirty=0
##QUEUE
##QFILE experiments/queue/001-arbiter-probe.toml
campaign = "arbiter-probe"
state = "queued"
##ENDQFILE
##END
"""
probe = C.parse_stage_probe(PROBE)
check(probe.branch, "feat/arbiter-probe", "the probe reads the branch")
check(probe.head[:7], "b093374", "and the full head")
check(probe.dirty, ["M src/main.cpp"], "and the modified files by name")
check([p["comm"] for p in probe.processes], ["counter"],
      "and the live engine, keyed on comm as everywhere else")
check(probe.binary["commit_short"], "b093374", "and the binary's commit")
check(probe.queue[0]["campaign"], "arbiter-probe",
      "and the host's queue, in the one round trip")

check(C.parse_stage_probe("##ERR not a directory: /nope").error,
      "not a directory: /nope", "a missing checkout reports rather than raises")
check_true(C.parse_stage_probe("##GIT\nmain\nabc").error,
           "a probe that never reached ##END is not a clean reading")

# A shell that prints nothing and exits zero returns no error either. Every
# field still has to read as absent rather than be missing, because the callers
# read them after the error guard and a KeyError mid-stage is the worst place
# for one.
empty_probe = C.parse_stage_probe("")
check_true(empty_probe.error, "an empty answer is an error, not a clean probe")
check((empty_probe.branch, empty_probe.binary, empty_probe.queue),
      ("?", {}, []), "and every field still answers")

# Each refusal is a way to destroy work that cannot be recovered from this
# side. The hosts sit on someone else's branch with a live run on one of them,
# so every one of these fires on the machines as they are.
def host_probe(**kwargs) -> C.HostProbe:
    base = {"branch": "feat/x", "head": "a" * 40,
            "binary": {"commit": "a" * 40, "commit_short": "aaaaaaa",
                       "dirty": "0"}}
    base.update(kwargs)
    return C.HostProbe(**base)


clean = host_probe()
check(C.stage_refusals(clean, "feat/x"), [],
      "a clean, idle host on the right branch is staged without a fight")
check([k for k, _ in C.stage_refusals(host_probe(dirty=[" M x.cpp"]),
                                      "feat/x")],
      ["dirty"], "a dirty checkout is refused")
check([k for k, _ in C.stage_refusals(
    host_probe(processes=[{"comm": "counter", "profile": None}]), "feat/x")],
    ["busy"], "a live run is refused")
check([k for k, _ in C.stage_refusals(clean, "feat/other")],
      ["branch"], "a host on another branch is refused")
check(len(C.stage_refusals(host_probe(dirty=[" M x"], processes=[
    {"comm": "counter", "profile": None}]), "feat/other")), 3,
    "and all three are reported at once, not one per attempt")

apply_forced = C.stage_apply_script("/r", "feat/x", "s" * 40, "make", True)
apply_plain = C.stage_apply_script("/r", "feat/x", "s" * 40, "make", False)
check_true("checkout -f" in apply_forced,
           "--force discards tracked modifications")
check_true("checkout -f" not in apply_plain,
           "and nothing else does")
check_true("git clean" not in apply_forced,
           "no git clean, ever: a host's untracked files are its results")
check_true("rev-list --count" in apply_plain,
           "an unforced stage refuses a checkout ahead of the pushed commit")
check_true(" -- -B " not in apply_plain,
           "`git checkout -- -B x` would read -B as a path name")
check_true("| tail" not in apply_forced.split("@")[0] + apply_forced,
           "a build piped into tail reports tail's exit status, so a failed "
           "build would read as a staged host")


# ── stage and start, end to end against a throwaway checkout ──────────────────

def git(root: Path, *args: str) -> str:
    """git against a fixture, with the ambient config that could break it off.

    Identity and signing are pinned rather than inherited: a machine whose
    global config sets `commit.gpgsign` cannot commit here without a key, and
    the fixture's whole purpose is to behave the same on every box.
    """
    proc = subprocess.run(
        ["git", "-C", str(root), "-c", "user.email=t@t", "-c", "user.name=t",
         "-c", "commit.gpgsign=false", "-c", "gpg.format=openpgp", *args],
        capture_output=True, text=True)
    if proc.returncode != 0:
        # Fatal, unlike an assertion: the fixture is unusable from here on.
        fail(f"git {' '.join(args)}: {proc.stderr.strip()}")
        raise SystemExit(1)
    return proc.stdout.strip()


def make_repo(root: Path, name: str, declaration: str, branch: str) -> None:
    """A checkout carrying one campaign declaration, on `branch`.

    With an origin to push to, because that is the route by which the branch
    (and the declaration on it) reaches a host: stage pushes here, the host
    fetches from here.
    """
    root.mkdir(parents=True, exist_ok=True)
    git(root, "init", "-q", ".")
    origin = root.with_name(root.name + "-origin.git")
    subprocess.run(["git", "init", "-q", "--bare", str(origin)], check=True)
    git(root, "remote", "add", "origin", str(origin))
    write_declaration(root, name, declaration)
    git(root, "add", "-A")
    # --no-verify: a globally configured core.hooksPath would otherwise run
    # this repo's own pre-commit inside a fixture that is not this repo.
    git(root, "commit", "-q", "--no-verify", "-m", "init")
    git(root, "checkout", "-q", "-b", branch)
    git(root, "checkout", "-q", "-")


def local_only(host: str, script: str, timeout: int = C.SSH_TIMEOUT_S):
    """Every host is this machine. No ssh, no lab machine, no launch."""
    return REAL_RUN_SHELL(C.LOCAL, script, timeout)


REAL_RUN_SHELL = C.run_shell
REAL_PROBE_PROCESSES = C.probe_processes

# The probe's `ps` is host-wide by design, and stage refuses on any live run
# whether or not it belongs to the checkout being staged, which is the reading
# a shared lab machine needs. A fixture pointed at a local checkout therefore
# inherits this machine's own counter and ltlsynt processes and refuses for
# reasons that have nothing to do with the fixture: the suite passed on an idle
# box and failed on a busy one. The list is supplied here instead, and both
# directions are asserted below, which makes ambient state a tested input.
FIXTURE_PROCESSES: list = []

FIXTURE_DECL = """
name = "fixture"
branch = "feat/fixture"
build = "true"
hosts = { av2 = "0-1", av3 = "2-3" }
phases = [ { profile = "full", jobs = 2 } ]
"""

# Stands in for build-release/counter, reporting the commit the checkout is
# standing on. Untracked, exactly as the real binary is, so it survives the
# checkout the stage script performs.
FAKE_BINARY = """#!/bin/sh
echo "commit=$(git rev-parse HEAD)"
echo "commit_short=$(git rev-parse --short HEAD)"
echo "dirty=0"
"""

stage_root = Path(tempfile.mkdtemp(prefix="campaign-stage-"))
try:
    repo = stage_root / "checkout"
    make_repo(repo, "fixture", FIXTURE_DECL, "feat/fixture")
    C.REPO_ROOT = repo
    C.run_shell = local_only
    C.source_path = lambda host: str(repo)
    C.probe_processes = lambda _lines: FIXTURE_PROCESSES

    def stage_args(**kwargs) -> argparse.Namespace:
        base = {"campaign": "fixture", "host": ["av2"], "force": False,
                "dry_run": True, "build_timeout": 60}
        base.update(kwargs)
        return argparse.Namespace(**base)

    # The checkout is on master/main, not on the campaign's branch, which is
    # the position both lab machines are in right now.
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_stage(stage_args())
    check(code, 1, "a host on another branch is not staged")
    check_true("REFUSED" in buffer.getvalue(), "and says so")
    check_true("--force" in buffer.getvalue(), "and names the way past it")

    git(repo, "checkout", "-q", "feat/fixture")
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_stage(stage_args())
    check(code, 0, "an idle host on the right branch stages")
    check_true("would stage" in buffer.getvalue(), "dry run, so nothing ran")

    # The other direction, from the same fixture: a run in flight refuses,
    # whether or not it belongs to this checkout. Resetting a shared machine
    # under somebody's run is what the refusal exists to prevent.
    FIXTURE_PROCESSES = [{"comm": "counter", "profile": None}]
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_stage(stage_args())
    check(code, 1, "a host with a live counter is not staged")
    check_true("busy" in buffer.getvalue(), "and busy is the reason given")
    FIXTURE_PROCESSES = []

    (repo / "experiments" / "fixture" / "campaign.toml").write_text(
        FIXTURE_DECL + "\n# touched\n")
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_stage(stage_args())
    check(code, 1, "a dirty host is not staged even on the right branch")
    check_true("campaign.toml" in buffer.getvalue(),
               "and the file it would discard is named")

    # --force without a terminal cannot confirm, so it refuses. That is what
    # keeps a scripted or cron-driven stage from resetting a host.
    stdin = sys.stdin
    sys.stdin = io.StringIO("av2\n")
    try:
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            code = C.cmd_stage(stage_args(force=True, dry_run=False))
        check(code, 1, "--force with nothing to confirm on must refuse")
        check_true("needs a terminal" in buffer.getvalue(),
                   "and say why, rather than proceeding")
        check_true("modified tracked file" in buffer.getvalue(),
                   "having first named what it would have discarded")
    finally:
        sys.stdin = stdin
    check(git(repo, "rev-parse", "--abbrev-ref", "HEAD"), "feat/fixture",
          "and the checkout is untouched")
    git(repo, "checkout", "-q", "--", ".")

    # The whole apply path, against a stub binary that reports the commit its
    # checkout is on: push, fetch, checkout, build, and the version read back.
    # The last step is the one that matters -- a campaign whose binary predates
    # its branch produces rows that look valid and name the wrong commit.
    binary = repo / C.COUNTER_BINARY
    binary.parent.mkdir(parents=True, exist_ok=True)
    binary.write_text(FAKE_BINARY)
    binary.chmod(0o755)
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_stage(stage_args(dry_run=False))
    check(code, 0, f"a clean host stages for real: {buffer.getvalue()}")
    check_true("staged" in buffer.getvalue()
               and "MISMATCH" not in buffer.getvalue(),
               "with the binary's commit matching the branch it was staged to")

    # A binary left behind by an earlier campaign must fail the same check.
    binary.write_text("#!/bin/sh\necho commit=deadbee\necho commit_short="
                      "deadbee\necho dirty=0\n")
    binary.chmod(0o755)
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_stage(stage_args(dry_run=False))
    check(code, 1, "a stale binary fails the stage")
    check_true("BINARY MISMATCH" in buffer.getvalue(), "and says which check")
    binary.write_text(FAKE_BINARY)
    binary.chmod(0o755)

    # start: the seed split comes from the declaration and from nowhere else.
    head = git(repo, "rev-parse", "HEAD")

    def start_args(**kwargs) -> argparse.Namespace:
        base = {"campaign": "fixture", "host": None, "dry_run": True,
                "ignore_queue": False}
        base.update(kwargs)
        return argparse.Namespace(**base)

    started = io.StringIO()
    with contextlib.redirect_stdout(started):
        code = C.cmd_start(start_args())
    printed = started.getvalue()
    check_true("--seeds 0 1" in printed and "--seeds 2 3" in printed,
               "each host is launched on its own declared seeds")
    check_true("--profile full" in printed, "with the declared profile")
    check(code, 0, "a staged host is ready to be launched on")

    # The runner's own freshness gate, asked one step early: a launch that
    # dies on the far side of a nohup leaves its message in a log nobody is
    # reading yet.
    binary.unlink()
    blocked = io.StringIO()
    with contextlib.redirect_stdout(blocked):
        code = C.cmd_start(start_args())
    check(code, 1, "a host with no binary is not launched on")
    check_true("no build-release/counter" in blocked.getvalue(),
               "and the missing binary is what it names")
    binary.write_text(FAKE_BINARY)
    binary.chmod(0o755)

    fixture = {"branch": "feat/fixture", "name": "fixture"}
    stale = C.start_refusals(
        C.HostProbe(branch="feat/fixture", head=head,
                    binary={"commit": "b" * 40, "commit_short": "bbbbbbb",
                            "dirty": "0"}), fixture, head)
    check(len(stale), 1, "a binary from another commit is the only complaint")
    check_true("bbbbbbb" in stale[0], "and it names the commit it was built at")
    ready = C.HostProbe(branch="feat/fixture", head=head,
                        binary={"commit": head, "commit_short": head[:7],
                                "dirty": "0"})
    busy = C.start_refusals(
        replace(ready, processes=[{"comm": "python3", "profile": "tlsf"}]),
        fixture, head)
    check_true(any("already live" in r for r in busy),
               "a host already running a campaign is not launched on again")

    # A pending queue entry is a launch waiting to happen: starting beside it
    # is two runners resuming off one CSV as soon as the tick fires. Same
    # reasoning as enqueue's refusal of a second entry, so the same default.
    queued = replace(ready, queue=[{"file": "003-fixture.toml",
                                    "campaign": "fixture", "state": "queued"}])
    racing = C.start_refusals(queued, fixture, head)
    check(len(racing), 1, "a queued entry for this campaign refuses the start")
    check_true("003-fixture.toml" in racing[0] and "queued" in racing[0],
               f"naming the entry and its state: {racing}")
    check_true("--ignore-queue" in racing[0], "and the way past it")
    check(C.start_refusals(queued, fixture, head, ignore_queue=True), [],
          "which the override clears")
    running_entry = C.start_refusals(
        replace(ready, queue=[{"file": "003-fixture.toml",
                               "campaign": "fixture", "state": "running"}]),
        fixture, head)
    check_true(len(running_entry) == 1 and "running" in running_entry[0],
               "a running entry refuses on the same ground, named as running")
    for spent in ("done", "failed"):
        check(C.start_refusals(
            replace(ready, queue=[{"file": "003-fixture.toml",
                                   "campaign": "fixture", "state": spent}]),
            fixture, head), [],
            f"a {spent} entry is not pending, so it does not block a start")
    check(C.start_refusals(
        replace(ready, queue=[{"file": "003-other.toml", "campaign": "other",
                               "state": "queued"}]), fixture, head), [],
        "and another campaign's entry is not this campaign's race")

    # End to end, so the refusal and the override are exercised through the
    # probe rather than only against a constructed one.
    (repo / C.QUEUE_DIR).mkdir(parents=True, exist_ok=True)
    (repo / C.QUEUE_DIR / "007-fixture.toml").write_text(
        C.dump_toml({"campaign": "fixture", "host": "av2", "state": "queued"}))
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_start(start_args())
    check(code, 1, "a start beside a pending entry is refused")
    check_true("007-fixture.toml" in buffer.getvalue(), "and names the entry")
    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_start(start_args(ignore_queue=True))
    check(code, 0, "--ignore-queue launches anyway")
    check_true("racing queue entry 007-fixture.toml" in buffer.getvalue(),
               "and still names what it is racing")
    shutil.rmtree(repo / C.QUEUE_DIR)

    chain = " && ".join(C.phase_command(p, [0, 1]) for p in
                        C.load_campaign("fixture", repo)["phases"])
    script = C.launch_script("/r", chain, "experiments/fixture/x.log")
    check_true("nohup" in script and "< /dev/null" in script,
               "the launch survives the ssh session that started it")
finally:
    C.run_shell = REAL_RUN_SHELL
    C.probe_processes = REAL_PROBE_PROCESSES
    shutil.rmtree(stage_root, ignore_errors=True)


# ── The queue ─────────────────────────────────────────────────────────────────

STUB_RUNNER = '''#!/usr/bin/env python3
"""Stands in for run_experiments.py: records its arguments, never runs one."""
import os, sys, time
here = os.path.dirname(os.path.abspath(__file__))


def control(name):
    path = os.path.join(here, name)
    return open(path).read().strip() if os.path.exists(path) else None


with open(os.path.join(here, "calls.txt"), "a") as handle:
    handle.write(" ".join(sys.argv[1:]) + "\\n")
if control("sleep"):
    time.sleep(float(control("sleep")))
sys.exit(int(control("exit-code") or 0))
'''

TWO_PHASE_DECL = """
name = "queued"
branch = "feat/queued"
hosts = { local = "0-3" }
phases = [ { profile = "full", jobs = 1 }, { profile = "tlsf", jobs = 2 } ]
"""

queue_root = Path(tempfile.mkdtemp(prefix="campaign-queue-"))
try:
    repo = queue_root / "checkout"
    make_repo(repo, "queued", TWO_PHASE_DECL, "feat/queued")
    default_branch = git(repo, "rev-parse", "--abbrev-ref", "HEAD")
    git(repo, "checkout", "-q", "feat/queued")
    control_dir = queue_root / "stub"
    control_dir.mkdir()
    stub = control_dir / "stub_runner.py"
    stub.write_text(STUB_RUNNER)
    calls = control_dir / "calls.txt"
    lock = queue_root / "queue.lock"

    C.REPO_ROOT = repo
    C.RUNNER_CMD = f"{sys.executable} {stub}"
    C.run_shell = local_only
    C.source_path = lambda host: str(repo)
    campaign = C.load_campaign("queued", repo)

    def tick_args(**kwargs) -> argparse.Namespace:
        base = {"host": "local", "root": str(repo), "lock": str(lock),
                "dry_run": False}
        base.update(kwargs)
        return argparse.Namespace(**base)

    def tick(**kwargs) -> tuple:
        buffer = io.StringIO()
        with contextlib.redirect_stdout(buffer):
            code = C.cmd_tick(tick_args(**kwargs))
        return code, buffer.getvalue()

    def only_entry() -> dict:
        entries = C.queue_entries(repo)
        check(len(entries), 1, "exactly one queue entry")
        return entries[0]

    buffer = io.StringIO()
    with contextlib.redirect_stdout(buffer):
        code = C.cmd_enqueue(argparse.Namespace(
            campaign="queued", host=["local"], max_attempts=2, again=False,
            dry_run=False))
    check(code, 0, "enqueue writes an entry")
    entry = only_entry()
    check(entry["file"], "001-queued.toml", "numbered from one")
    check(entry["state"], "queued", "and starts queued")
    check(entry["seeds"], "0-3", "carrying the host's declared seeds")
    check(entry["phases"], 2, "and the phase count it was declared with")

    # A campaign already queued is not queued twice by accident: two entries
    # for one campaign is two runners on one results CSV.
    with contextlib.redirect_stdout(io.StringIO()):
        code = C.cmd_enqueue(argparse.Namespace(
            campaign="queued", host=["local"], max_attempts=2, again=False,
            dry_run=False))
    check(code, 1, "a second enqueue is refused")
    check(len(C.queue_entries(repo)), 1, "and writes nothing")

    # Lock contention. The crontab's flock -n is the outer guard; this is the
    # inner one, which is what stops a tick typed by hand racing the cron tick
    # into a second runner over the same CSV.
    held = C.acquire_lock(lock)
    check_true(held is not None, "the lock is free to start with")
    check_true(C.acquire_lock(lock) is None, "and exclusive once taken")
    code, printed = tick()
    check(code, 0, "a tick that cannot take the lock exits quietly")
    check_true("is held" in printed, "saying that another tick has it")
    check(only_entry()["state"], "queued", "and changes nothing")
    check_true(not calls.exists(), "and runs nothing")
    assert held is not None  # checked above; narrows for the type checker
    held.close()

    code, printed = tick()
    check(code, 0, "the first real tick runs phase 0")
    entry = only_entry()
    check(entry["state"], "queued", "and requeues for the second phase")
    check(entry["phase"], 1, "having advanced the phase index")
    check(calls.read_text().count("\n"), 1, "one runner invocation")
    check_true("--profile full --jobs 1 --seeds 0 1 2 3"
               in calls.read_text(), "on the declared profile and seeds")

    code, printed = tick()
    check(code, 0, "the second tick runs the last phase")
    entry = only_entry()
    check(entry["state"], "done", "which finishes the entry")
    check_true("--profile tlsf --jobs 2" in calls.read_text(),
               "having run the second phase, not the first again")

    code, printed = tick()
    check_true("nothing queued" in printed, "a done entry is not run again")
    check(calls.read_text().count("\n"), 2, "and no third invocation")

    # A tick killed mid-phase must cost nothing. The entry is left `running`,
    # and the next tick puts it back in the queue and runs the same phase over
    # the same seeds -- run_experiments.py resumes off the results CSV, so the
    # work already done is not repeated.
    calls.unlink()
    (control_dir / "sleep").write_text("30")
    entry = only_entry()
    entry.update({"state": "queued", "phase": 0, "attempts": 0})
    C.write_entry(entry["path"], entry)

    env = dict(os.environ, COUNTER_RUNNER_CMD=f"{sys.executable} {stub}")
    child = subprocess.Popen(
        [sys.executable, str(CAMPAIGN_PY), "tick",
         "--root", str(repo), "--lock", str(lock), "--host", "local"],
        env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    deadline = time.time() + 60
    while time.time() < deadline and only_entry()["state"] != "running":
        time.sleep(0.1)
    check(only_entry()["state"], "running", "the tick marks the entry running")
    child.send_signal(signal.SIGKILL)
    child.wait(timeout=30)
    check(only_entry()["state"], "running",
          "and a killed tick leaves it that way")

    (control_dir / "sleep").unlink()
    code, printed = tick()
    check_true("was left running" in printed,
               "the next tick recovers the interrupted entry")
    entry = only_entry()
    check(entry["phase"], 1, "and the recovered phase ran to completion")
    check(entry["state"], "queued", "leaving the second phase to run")
    invocations = [ln for ln in calls.read_text().splitlines() if ln]
    check(len(invocations), 2, "the interrupted phase was attempted twice")
    check(invocations[0], invocations[1],
          "over exactly the same selection, so the runner resumes it")

    # A failing phase is visible rather than retried for ever.
    calls.unlink()
    (control_dir / "exit-code").write_text("3")
    entry = only_entry()
    entry.update({"state": "queued", "phase": 0, "attempts": 0,
                  "max_attempts": 2})
    C.write_entry(entry["path"], entry)
    code, printed = tick()
    check(code, 1, "a failed phase fails the tick")
    entry = only_entry()
    check(entry["state"], "queued", "the first failure is retried")
    check(entry["attempts"], 1, "with the attempt counted")
    code, printed = tick()
    entry = only_entry()
    check(entry["state"], "failed", "the cap stops it")
    check_true("exited 3" in entry["last_error"], "and records why")
    code, printed = tick()
    check_true("nothing queued" in printed, "a failed entry stops moving")
    check(len(calls.read_text().splitlines()), 2,
          "so the machine is not spent re-running it")

    with contextlib.redirect_stdout(io.StringIO()):
        check(C.cmd_requeue(argparse.Namespace(host=C.LOCAL,
                                               entry="001-queued.toml")), 0,
              "requeue clears a failed entry by hand")
    entry = only_entry()
    check((entry["state"], entry["attempts"]), ("queued", 0),
          "which is the only way out of failed")

    # An entry whose branch the checkout has left must not run: its rows would
    # come from code the campaign never declared.
    (control_dir / "exit-code").unlink()
    calls.unlink()
    git(repo, "checkout", "-q", default_branch)
    code, printed = tick()
    check(code, 1, "a checkout on the wrong branch does not run a phase")
    check_true("stage it first" in only_entry()["last_error"],
               "and the entry says what to do about it")
    check_true(not calls.exists(), "and nothing was run")
    git(repo, "checkout", "-q", "feat/queued")

    rows = C.queue_rows([{"host": "local", "queue": [C.entry_body(only_entry())]}])
    check(rows[0][3], "queued", "the queue table reports the state")
    check(rows[0][4], "0/2", "the next phase against the total")

    line = C.cron_line("av2", "/home/benandrew/projects/counter")
    check_true("flock -n" in line and "tick --host av2" in line,
               "the crontab line locks and names its host")
    check_true("*/5 * * * *" in line, "every five minutes")
    check_true("python3" in line,
               "with python3: av2's shell has no `python` at all")
finally:
    C.run_shell = REAL_RUN_SHELL
    shutil.rmtree(queue_root, ignore_errors=True)


if FAILURES:
    print(f"\n{len(FAILURES)} campaign.py test(s) failed.")
    sys.exit(1)
print("All campaign.py tests passed.")
