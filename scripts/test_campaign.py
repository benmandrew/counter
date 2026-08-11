#!/usr/bin/env python3
"""Tests for campaign.py's parsing, process detection and collect verification.

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
duplicates keys looks exactly like a collect that worked.
"""

import argparse
import contextlib
import csv
import io
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import campaign as C  # noqa: E402
import merge_experiments as merge  # noqa: E402


def check(got, want, msg):
    if got != want:
        print(f"FAIL: {msg}\n  got:  {got!r}\n  want: {want!r}")
        sys.exit(1)


def check_true(cond, msg):
    if not cond:
        print(f"FAIL: {msg}")
        sys.exit(1)


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

print("All campaign.py tests passed.")
