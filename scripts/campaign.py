#!/usr/bin/env python3
"""Inspect and collect experiment campaigns running on the lab machines.

Two verbs so far:

    python scripts/campaign.py status            # one table of live state
    python scripts/campaign.py status --json     # the same, machine-readable
    python scripts/campaign.py collect --profile tlsf --dry-run
    python scripts/campaign.py collect --profile tlsf

``status`` is read-only. It asks each host for its launch manifests
(``experiments/<stem>-manifest-<host>.json``, written by run_experiments.py),
its git state, its process table and the mtimes underneath each campaign's
result directory, then asks ``run_experiments.py --dry-run`` on that same host
how many rows the launch's selection plans. Nothing here re-derives a sweep:
the manifest says what the launch asked for and the runner says how many rows
that expands to, so a profile edit cannot make this script disagree with the
runner about the size of a campaign.

``collect`` wraps merge_experiments.py rather than reimplementing it -- the
rsync, the natural key and the merge all come from that module -- and adds the
verification the manual step never had: row counts per host against the merged
total, and a duplicate-key scan.

Hosts are reached by their ssh-config alias (``av2``), not by the FQDN in
merge_experiments.REMOTES: the FQDN form falls through to password auth under
BatchMode, which a status poll cannot answer.
"""

import argparse
import json
import shlex
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import merge_experiments as merge  # noqa: E402

REPO_ROOT = Path(__file__).parent.parent

# The lab machines, by ssh-config alias. Bare aliases deliberately: see the
# module docstring. The repo path on each is merge_experiments.REMOTE_ROOT.
HOSTS: tuple[str, ...] = ("av2", "av3")
LOCAL = "local"

SSH_OPTS = ["-o", "ConnectTimeout=8", "-o", "BatchMode=yes"]
SSH_TIMEOUT_S = 45
# collect --dry-run's measure step walks a whole result tree with du and find.
# On a cold cache over the 29GB experiments dir that outlasts the status
# deadline by a wide margin, and a status deadline applied here reports a
# perfectly healthy machine as unreachable.
MEASURE_TIMEOUT_S = 600

# How long a campaign's newest run.log may go untouched before a live process
# stops corroborating it. Bounded by the harness's own limits rather than
# guessed: no single run outlasts them, and the largest any profile allows is
# a 3600s counter timeout (gen40, and the largest timeout_caps entry) plus an
# 1800s compare timeout, so 90 minutes bounds one run's silence. Doubled, for
# the granularity of the log writes and for a jobs=1 host, where the next
# run.log only appears once the previous run has finished.
STALE_RUN_S = 3 * 3600

# av2's non-interactive ssh shell has no `python` on PATH, only `python3`.
REMOTE_PYTHON = "python3"

# Marker protocol for the remote scripts. Chosen over one-shot JSON because the
# remote side has to stay a plain shell script: an inline python heredoc would
# need quoting through ssh and would hide its own parse errors as empty output.
MARK = "##"


def source_root(host: str) -> str:
    """rsync/ssh root of a host's counter checkout.

    The single place a host name becomes a transfer source, which is what lets
    the tests point collect at a local fixture instead of a lab machine.
    """
    return f"{host}:{merge.REMOTE_ROOT}"


def source_path(host: str) -> str:
    """Filesystem path of that checkout, for a shell probe run on the host."""
    _, sep, path = source_root(host).partition(":")
    return path if sep else source_root(host)


# -- Remote inventory ---------------------------------------------------------

INVENTORY_SCRIPT = r"""
cd @ROOT@ 2>/dev/null || { echo "@M@ERR not a directory: @ROOT@"; exit 3; }
echo "@M@PS"
ps -o comm=,args= -u "$(id -un)" 2>/dev/null
echo "@M@HOST"
hostname
date +%s
echo "@M@GIT"
git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "?"
git rev-parse --short HEAD 2>/dev/null || echo "?"
git status --porcelain --untracked-files=no 2>/dev/null | wc -l
echo "@M@MANIFESTS"
find experiments -maxdepth 1 -type f -name '*-manifest-*.json' 2>/dev/null |
while IFS= read -r m; do
  echo "@M@FILE $m"
  cat "$m"
  echo "@M@ENDFILE"
done
echo "@M@END"
"""
# The manifest sweep goes through `find`, not a glob, because the lab login
# shell is zsh: its default NOMATCH aborts the whole script where a glob matches
# nothing, so on a host with no manifest every section after the loop vanishes.
# sh and bash substitute the unmatched pattern instead, which is why this only
# ever fails in production.


def inventory_script(root: str) -> str:
    return INVENTORY_SCRIPT.replace("@ROOT@", shlex.quote(root)).replace("@M@", MARK)


def plan_args(campaign: dict) -> str:
    """``--dry-run`` arguments replaying one manifest's selection."""
    args = ["--profile", campaign["profile"], "--dry-run"]
    if campaign.get("sweeps"):
        args += ["--sweeps", *campaign["sweeps"]]
    if campaign.get("specs"):
        args += ["--specs", *campaign["specs"]]
    if campaign.get("seeds"):
        args += ["--seeds", *[str(s) for s in campaign["seeds"]]]
    return " ".join(shlex.quote(a) for a in args)


def detail_script(root: str, campaigns: list[dict]) -> str:
    """Shell reporting rows, mtimes and the runner's own plan per campaign.

    The plan comes from ``run_experiments.py --dry-run`` with the manifest's
    selection replayed onto it, so the count is whatever the runner would
    itself execute rather than a second expansion of the sweep. A profile the
    host's checkout no longer defines simply yields no plan line, and the
    caller reports that campaign's total as unknown.
    """
    lines = [f"cd {shlex.quote(root)} 2>/dev/null || exit 3"]
    for c in campaigns:
        q_csv = shlex.quote(c["results_csv"])
        q_dir = shlex.quote(c["results_dir"])
        lines += [
            f'echo "{MARK}CAMPAIGN {c["profile"]}"',
            f"if [ -f {q_csv} ]; then",
            f'  echo "{MARK}CSVROWS $(wc -l < {q_csv})"',
            f'  echo "{MARK}CSVMTIME $(stat -c %Y {q_csv})"',
            "fi",
            f"if [ -d {q_dir} ]; then",
            f'  echo "{MARK}LOGMTIME $(find {q_dir} -maxdepth 2 -name run.log '
            "-printf '%T@\\n' 2>/dev/null | sort -rn | head -1)\"",
            f'  echo "{MARK}RUNDIRS $(find {q_dir} -maxdepth 1 -mindepth 1 '
            '-type d 2>/dev/null | wc -l)"',
            "fi",
            f"{REMOTE_PYTHON} scripts/run_experiments.py {plan_args(c)} 2>/dev/null"
            f' | sed -n "s/^Plan:/{MARK}PLAN/p" | head -1',
        ]
    lines.append(f'echo "{MARK}END"')
    return "\n".join(lines)


TIMEOUT_PREFIX = "timed out after"


def is_timeout(error: str | None) -> bool:
    """Whether a run_shell error was the deadline rather than a failure.

    The two want different words in the output and often different fixes: a
    timeout on a healthy machine means the probe was too cheap a deadline for
    the tree it walked, not that the host is down.
    """
    return bool(error) and error.startswith(TIMEOUT_PREFIX)


def run_shell(host: str, script: str, timeout: int = SSH_TIMEOUT_S):
    """Run a shell script on ``host`` (or locally). Returns (stdout, error).

    A timeout returns whatever had already arrived alongside the error, rather
    than discarding it. The markers stream out in order, so a probe cut short
    still carries every section that completed, and throwing them away flips a
    finished campaign to "stalled" purely because the poll was slow.
    """
    if host == LOCAL:
        cmd = ["sh", "-c", script]
    else:
        cmd = ["ssh", *SSH_OPTS, host, script]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        partial = exc.stdout or ""
        if isinstance(partial, bytes):
            partial = partial.decode("utf-8", "replace")
        return (partial or None), f"{TIMEOUT_PREFIX} {timeout}s"
    except OSError as exc:
        return None, str(exc)
    if proc.returncode != 0 and not proc.stdout.strip():
        tail = [ln.strip() for ln in (proc.stderr or "").splitlines() if ln.strip()]
        return None, tail[-1] if tail else f"exit {proc.returncode}"
    return proc.stdout, None


def parse_inventory(text: str) -> dict:
    """Split the inventory script's marker-delimited output into fields."""
    out: dict = {"ps": [], "manifests": [], "hostname": "?", "epoch": None,
                 "branch": "?", "head": "?", "dirty": None, "error": None}
    section = None
    manifest_lines: list[str] = []
    buf: list[str] = []

    def flush() -> None:
        if section == "host":
            out["hostname"] = buf[0].strip() if buf else "?"
            if len(buf) > 1 and buf[1].strip().isdigit():
                out["epoch"] = int(buf[1].strip())
        elif section == "git" and len(buf) >= 3:
            out["branch"], out["head"] = buf[0].strip(), buf[1].strip()
            out["dirty"] = buf[2].strip().isdigit() and int(buf[2].strip()) > 0

    for line in text.splitlines():
        if line.startswith(MARK + "ERR"):
            out["error"] = line[len(MARK) + 3:].strip()
            return out
        if line.startswith(MARK + "FILE "):
            section, manifest_lines = "file", []
            continue
        if line.startswith(MARK + "ENDFILE"):
            try:
                out["manifests"].append(json.loads("\n".join(manifest_lines)))
            except json.JSONDecodeError:
                pass
            section = "manifests"
            continue
        if line.startswith(MARK):
            flush()
            section, buf = line[len(MARK):].strip().lower(), []
            continue
        if section == "ps":
            out["ps"].append(line)
        elif section == "file":
            manifest_lines.append(line)
        else:
            buf.append(line)
    flush()
    return out


def parse_plan_line(line: str) -> dict:
    """Pull the counts out of run_experiments.py's ``Plan:`` line.

    Format: ``N result rows (M via aliasing), K already done; L runs to
    execute``. Each number is read from the phrase that names it rather than
    from a fixed position, so a clause added to that line later does not
    silently reassign the ones already there.
    """
    out: dict = {}
    for phrase, key in (("result rows", "rows_planned"),
                        ("already done", "rows_done_plan"),
                        ("runs to execute", "runs_left")):
        idx = line.find(phrase)
        if idx < 0:
            continue
        words = line[:idx].replace("(", " ").replace(";", " ").split()
        for word in reversed(words):
            if word.isdigit():
                out[key] = int(word)
                break
    return out


def parse_detail(text: str) -> dict:
    """Marker output of detail_script, keyed by profile name.

    ``rows_done`` is the runner's own already-done count, out of the ``Plan:``
    line, which is scoped to the same selection ``rows_planned`` is. The whole
    CSV's length is not that number and cannot be substituted for it: profiles
    share CSVs (``replicate``, ``replicate-wkoff`` and ``replicate-recap`` all
    write ``results-replicate.csv``), and a top-up relaunch rewrites the
    manifest to a couple of seeds while the file still holds every earlier row,
    which read the file's length as progress reports 400/40 and calls a
    just-started campaign finished. The length is kept only as a fallback for
    when no plan came back, and a fallback row is marked in the output.
    """
    per: dict = {}
    current: dict | None = None
    for line in text.splitlines():
        if not line.startswith(MARK):
            continue
        tag, _, rest = line[len(MARK):].partition(" ")
        rest = rest.strip()
        if tag == "CAMPAIGN":
            current = per.setdefault(rest, {})
        elif current is None:
            continue
        elif tag == "CSVROWS" and rest.isdigit():
            # wc -l counts the header too; a header-only CSV has zero rows.
            current["csv_rows"] = max(0, int(rest) - 1)
        elif tag == "CSVMTIME" and rest.isdigit():
            current["csv_mtime"] = int(rest)
        elif tag == "LOGMTIME" and rest:
            try:
                current["log_mtime"] = int(float(rest))
            except ValueError:
                pass
        elif tag == "RUNDIRS" and rest.isdigit():
            current["run_dirs"] = int(rest)
        elif tag == "PLAN":
            current["plan"] = rest
            current.update(parse_plan_line(rest))
    for entry in per.values():
        if "rows_done_plan" in entry:
            entry["rows_done"] = entry["rows_done_plan"]
        elif "csv_rows" in entry:
            entry["rows_done"] = entry["csv_rows"]
            entry["rows_from_csv"] = True
    return per


# -- Process detection --------------------------------------------------------

ENGINE_COMMS = ("counter", "compare", "ltlsynt", "black")


def live_processes(ps_lines: list[str]) -> list[dict]:
    """Runner and engine processes, from a ``comm args`` process listing.

    Matched on ``comm``, never on the whole command line. ``pgrep -f
    run_experiments.py`` would match this script's own ssh command, whose text
    names it; the comm of that process is the login shell, so keying on comm
    excludes it by construction.
    """
    found = []
    for line in ps_lines:
        comm, _, args = line.strip().partition(" ")
        args = args.strip()
        if comm in ENGINE_COMMS:
            found.append({"comm": comm, "profile": None, "args": args})
        elif comm.startswith("python") and "run_experiments.py" in args:
            if "--dry-run" in args:
                continue
            found.append({"comm": comm, "profile": profile_of_args(args),
                          "args": args})
    return found


def profile_of_args(args: str) -> str:
    try:
        words = shlex.split(args)
    except ValueError:
        words = args.split()
    for i, word in enumerate(words):
        if word == "--profile" and i + 1 < len(words):
            return words[i + 1]
        if word.startswith("--profile="):
            return word.split("=", 1)[1]
    return "full"


# -- Gathering ----------------------------------------------------------------

def campaigns_from_manifests(manifests: list[dict]) -> list[dict]:
    """One record per manifest, flattened to the fields status reports on."""
    out = []
    for m in manifests:
        sweep = m.get("sweep") or {}
        git = m.get("git") or {}
        counter = (m.get("binaries") or {}).get("counter") or {}
        out.append({
            "profile": m.get("profile", "?"),
            "manifest_host": m.get("hostname", "?"),
            "started": m.get("started"),
            "branch": git.get("branch", "?"),
            "head": (git.get("head") or "?")[:7],
            "binary_commit": counter.get("commit_short", "?"),
            # The runner refuses to launch off a dirty binary without
            # --allow-stale-binary, so a true here says the override was used
            # and the campaign's rows name a commit they did not come from.
            "dirty_binary": counter.get("dirty") == "1",
            "results_csv": sweep.get("results_csv", ""),
            "results_dir": sweep.get("results_dir", ""),
            "sweeps": sweep.get("sweeps"),
            "specs": sweep.get("specs") or [],
            "seeds": sweep.get("seeds") or [],
            "jobs": sweep.get("jobs"),
        })
    out.sort(key=lambda c: (c.get("started") or "", c["profile"]))
    return out


def gather_host(host: str, root: str, only: str | None, want_plan: bool,
                show_all: bool) -> dict:
    """Everything status reports for one host.

    Never raises: an unreachable machine costs one row of the table, not the
    run, so a poll against a lab that is half up still says what the other half
    is doing.
    """
    report: dict = {"host": host, "root": root, "reachable": False,
                    "error": None, "campaigns": [], "hidden": 0,
                    "processes": []}
    text, err = run_shell(host, inventory_script(root))
    inv = parse_inventory(text or "")
    if inv["error"]:
        report["error"] = inv["error"]
        return report
    # A timed-out inventory is still usable when its git section completed --
    # the sections stream in order, so a branch means everything before it
    # arrived. Without one there is nothing to report but the failure.
    if err is not None:
        if not (is_timeout(err) and inv["branch"] != "?"):
            report["error"] = err
            return report
        report["partial"] = True
    report.update({
        "reachable": True,
        "hostname": inv["hostname"],
        "epoch": inv["epoch"] or int(time.time()),
        "branch": inv["branch"],
        "head": inv["head"],
        "dirty": inv["dirty"],
        "processes": live_processes(inv["ps"]),
    })
    campaigns = campaigns_from_manifests(inv["manifests"])
    if only:
        campaigns = [c for c in campaigns if c["profile"] == only]
    elif not show_all:
        # Manifests accumulate: a machine keeps one per campaign it has ever
        # launched. The ones whose branch the checkout has since left cannot be
        # the campaign now in flight, and their profile is usually no longer
        # defined either, so they report as unknowns and drown the live rows.
        live = [c for c in campaigns if c["branch"] == report["branch"]]
        report["hidden"] = len(campaigns) - len(live)
        campaigns = live
    if campaigns and want_plan:
        text, err = run_shell(host, detail_script(root, campaigns))
        # Parsed unconditionally: the campaigns are reported one after another,
        # so a probe cut short still holds complete records for the ones that
        # got their turn, and dropping them reports those as stalled.
        detail = parse_detail(text or "")
        if err is not None:
            report["partial"] = True
        for c in campaigns:
            c.update(detail.get(c["profile"], {}))
    for c in campaigns:
        annotate(c, report)
    report["campaigns"] = campaigns
    return report


def annotate(c: dict, host_report: dict) -> None:
    """Derive percent, staleness, state and ETA for one campaign.

    Times are differenced against the host's own clock, sampled in the same
    inventory call, because av3 runs minutes away from av2 whenever NTP is off
    and a staleness measured across that skew is fiction.

    A process is attributed to a campaign only when it names that campaign's
    profile. An engine process names none, and matching those against every
    campaign made one unrelated `counter` on av2 report all six of its archived
    campaigns as running while idle av3 reported the same six from the same
    data as stalled. Unattributable processes belong in the host's PROCESSES
    column, which is where they still appear.

    A matched process is corroborated against the campaign's own output before
    it counts as progress. `running` beside a nine-day staleness is the reading
    that stops someone investigating a dead run, which is the failure this tool
    exists to prevent, so that combination reports as `stuck` instead — worth
    seeing rather than hidden behind either plain state.
    """
    now = host_report.get("epoch") or int(time.time())
    done = c.get("rows_done")
    planned = c.get("rows_planned")
    c["pct"] = (100.0 * done / planned) if done is not None and planned else None
    c["stale_s"] = now - c["log_mtime"] if c.get("log_mtime") else None
    c["running"] = any(p["profile"] and p["profile"] == c["profile"]
                       for p in host_report["processes"])
    started = parse_started(c.get("started"))
    c["elapsed_s"] = int(now - started) if started else None

    # A log with no mtime at all is a campaign whose first run directory does
    # not exist yet, so silence there is the launch, not a stall.
    silent = c["stale_s"] is not None and c["stale_s"] > STALE_RUN_S
    if planned is not None and done is not None and done >= planned:
        c["state"] = "done"
    elif c["running"] and silent:
        c["state"] = "stuck"
    elif c["running"]:
        c["state"] = "running"
    else:
        c["state"] = "stalled"

    c["eta_s"] = None
    if (c["state"] == "running" and done and planned
            and c["elapsed_s"] and c["elapsed_s"] > 0):
        # Deliberately crude: rows per second since the manifest was written,
        # which credits this launch with any rows it merely resumed onto. It
        # answers "roughly when", not "when".
        rate = done / c["elapsed_s"]
        if rate > 0:
            c["eta_s"] = int((planned - done) / rate)


def parse_started(value: str | None) -> float | None:
    if not value:
        return None
    for fmt in ("%Y-%m-%dT%H:%M:%S%z", "%Y-%m-%dT%H:%M:%S"):
        try:
            return datetime.strptime(value, fmt).timestamp()
        except ValueError:
            continue
    return None


# -- Formatting ---------------------------------------------------------------

def human_duration(seconds: float | None) -> str:
    if seconds is None:
        return "-"
    seconds = int(seconds)
    if seconds < 60:
        return f"{seconds}s"
    if seconds < 3600:
        return f"{seconds // 60}m"
    if seconds < 86400:
        return f"{seconds // 3600}h{(seconds % 3600) // 60:02d}m"
    return f"{seconds // 86400}d{(seconds % 86400) // 3600:02d}h"


def human_bytes(n: int | None) -> str:
    if n is None:
        return "-"
    size = float(n)
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if size < 1024 or unit == "TB":
            return f"{int(size)}B" if unit == "B" else f"{size:.1f}{unit}"
        size /= 1024
    return "-"


def render_table(rows: list[list[str]], headers: list[str]) -> str:
    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))
    out = ["  ".join(h.ljust(w) for h, w in zip(headers, widths)).rstrip()]
    for row in rows:
        out.append("  ".join(c.ljust(w) for c, w in zip(row, widths)).rstrip())
    return "\n".join(out)


HEADERS = ["HOST", "CAMPAIGN", "ROWS", "PCT", "STATE", "STALE", "ETA",
           "BRANCH", "BINARY"]


def status_rows(reports: list[dict]) -> list[list[str]]:
    rows = []
    for r in reports:
        if not r["reachable"]:
            # Truncated: an ssh failure can be a paragraph, and one wide cell
            # widens the whole column for every other host.
            why = (r["error"] or "?").replace("\t", " ")
            if len(why) > 44:
                why = why[:41] + "..."
            label = "no answer" if is_timeout(r["error"]) else "unreachable"
            rows.append([r["host"], f"({label}: {why})",
                         "-", "-", "-", "-", "-", "-", "-"])
            continue
        if not r["campaigns"]:
            note = "(busy, no manifest)" if r["processes"] else "(no campaigns)"
            if r.get("hidden"):
                note = f"(none on this branch, {r['hidden']} older)"
            rows.append([r["host"], note, "-", "-", "-", "-", "-", "-", "-"])
            continue
        for c in r["campaigns"]:
            done = c.get("rows_done")
            planned = c.get("rows_planned")
            # The campaign's own branch, out of its manifest, flagged when the
            # checkout has since moved off it -- that disagreement is exactly
            # what makes a resumed campaign produce rows from another commit.
            branch = c["branch"] + ("!" if c["branch"] != r["branch"] else "")
            # "~" marks a row whose done count is the whole CSV rather than
            # this launch's share of it, which is the one row that can still
            # overstate progress. See parse_detail.
            done_cell = ("~" if c.get("rows_from_csv") else "") + (
                str(done) if done is not None else "?")
            rows.append([
                r["host"],
                c["profile"],
                f"{done_cell}/{planned if planned is not None else '?'}",
                # Truncated, not rounded: 797/800 reading "100%" is the one
                # number a status poll must not get wrong.
                f"{int(c['pct'])}%" if c.get("pct") is not None else "-",
                c["state"],
                human_duration(c.get("stale_s")),
                human_duration(c.get("eta_s")),
                branch,
                c["binary_commit"] + ("*" if c.get("dirty_binary") else ""),
            ])
    return rows


def status_notes(reports: list[dict]) -> list[str]:
    notes = []
    for r in reports:
        if not r["reachable"]:
            continue
        if r.get("hidden"):
            notes.append(f"{r['host']}: {r['hidden']} manifest(s) from other "
                         f"branches hidden — pass --all to see them")
        for p in r["processes"]:
            if p["profile"]:
                notes.append(f"{r['host']}: runner live on "
                             f"--profile {p['profile']}")
        for c in r["campaigns"]:
            if c["state"] == "stuck":
                notes.append(
                    f"{r['host']}/{c['profile']}: a runner is alive on this "
                    f"profile but its newest run.log has not been touched for "
                    f"{human_duration(c['stale_s'])} — longer than any single "
                    f"run may take, so it is producing nothing")
            if c["branch"] != r["branch"]:
                notes.append(f"{r['host']}/{c['profile']}: launched on "
                             f"{c['branch']}, checkout now on {r['branch']} (!)")
            if c.get("dirty_binary"):
                notes.append(f"{r['host']}/{c['profile']}: launched off a "
                             f"binary built dirty (* on BINARY)")
            if c.get("rows_planned") is None:
                notes.append(f"{r['host']}/{c['profile']}: no plan — that "
                             f"checkout may no longer define the profile")
            if c.get("rows_from_csv"):
                notes.append(
                    f"{r['host']}/{c['profile']}: ~ROWS counts the whole CSV, "
                    f"not this launch's selection — it overstates progress "
                    f"where profiles share a CSV or a relaunch topped up")
        if r.get("partial"):
            notes.append(f"{r['host']}: probe timed out; this host's rows are "
                         f"whatever had arrived by then")
    return list(dict.fromkeys(notes))


CHECKOUT_HEADERS = ["HOST", "MACHINE", "BRANCH", "HEAD", "TREE", "PROCESSES"]


def checkout_rows(reports: list[dict]) -> list[list[str]]:
    """The checkout each host is sitting on, independent of any campaign.

    Separate from the campaign table because the two can disagree: a campaign
    row names the branch its manifest was written on, which is the branch its
    results came from, while this names the branch a resumed run would use.
    """
    rows = []
    for r in reports:
        if not r["reachable"]:
            rows.append([r["host"], "-", "-", "-", "-", "unreachable"])
            continue
        procs = ", ".join(dict.fromkeys(p["comm"] for p in r["processes"]))
        rows.append([r["host"], r.get("hostname", "?"), r["branch"], r["head"],
                     "dirty" if r.get("dirty") else "clean", procs or "idle"])
    return rows


def print_status(reports: list[dict]) -> None:
    print(render_table(checkout_rows(reports), CHECKOUT_HEADERS))
    print()
    print(render_table(status_rows(reports), HEADERS))
    notes = status_notes(reports)
    if notes:
        print()
        for note in notes:
            print(f"note: {note}")
    print("\nBRANCH is the branch the campaign was launched on, per its "
          "manifest, and is flagged\nwith ! where the checkout has since moved "
          "off it. ROWS is done against planned, both\nfrom the plan "
          "run_experiments.py reports for that manifest's selection; a ~ marks "
          "a\ndone count taken from the whole CSV because no plan came back. "
          "STALE is the age of\nthe newest run.log under the campaign's "
          "results dir, on the host's own clock.\nSTATE is running only where "
          "a runner names this profile and the log is fresher than\n"
          f"{human_duration(STALE_RUN_S)}; stuck means the runner is there and "
          "the log is not moving. ETA\nextrapolates rows-so-far over time "
          "since the manifest was written, and is crude by\nconstruction.")


def cmd_status(args: argparse.Namespace) -> int:
    hosts = [args.host] if args.host else list(HOSTS)
    targets = [(h, source_path(h)) for h in hosts if h != LOCAL]
    if not args.host or args.host == LOCAL:
        targets.append((LOCAL, str(REPO_ROOT)))
    with ThreadPoolExecutor(max_workers=max(1, len(targets))) as pool:
        reports = list(pool.map(
            lambda t: gather_host(t[0], t[1], args.campaign, not args.no_plan,
                                  args.all),
            targets))
    if args.json:
        print(json.dumps({"generated": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
                          "hosts": reports}, indent=2))
    else:
        print_status(reports)
    return 0 if all(r["reachable"] for r in reports) else 1


# -- collect ------------------------------------------------------------------

def resolve_profile(args: argparse.Namespace) -> tuple[str, str | None]:
    """``(csv_name, result_dir_name)`` for the profile, or explicit overrides.

    merge_experiments.PROFILE_CSVS is the authority. A campaign living on an
    unmerged branch is absent from it, which is what the overrides are for
    rather than a guess at ``results-<profile>.csv``: a wrong guess merges a
    campaign into a file nobody is watching, and says nothing while it does.
    """
    if args.csv or args.results_dir:
        if not args.csv:
            sys.exit("--results-dir needs --csv alongside it")
        default_dir = Path(args.csv).stem
        return args.csv, (None if args.no_results
                          else (args.results_dir or default_dir))
    if args.profile not in merge.PROFILE_CSVS:
        sys.exit(f"Unknown profile {args.profile!r}. Known: "
                 f"{', '.join(sorted(merge.PROFILE_CSVS))}\n"
                 f"For a campaign on an unmerged branch, pass "
                 f"--csv results-<name>.csv [--results-dir results-<name>].")
    return (merge.PROFILE_CSVS[args.profile],
            None if args.no_results else merge.PROFILE_RESULT_DIRS[args.profile])


MEASURE_SCRIPT = r"""
cd @ROOT@ 2>/dev/null || { echo "@M@ERR not a directory: @ROOT@"; exit 3; }
if [ -f @CSV@ ]; then
  echo "@M@CSVROWS $(wc -l < @CSV@)"
  echo "@M@CSVBYTES $(stat -c %s @CSV@)"
else
  echo "@M@NOCSV"
fi
if [ -d @DIR@ ]; then
  echo "@M@DIRBYTES $(du -sb @DIR@ | cut -f1)"
  echo "@M@DIRFILES $(find @DIR@ -type f | wc -l)"
fi
echo "@M@END"
"""

MEASURE_KEYS = {"CSVROWS": "csv_rows", "CSVBYTES": "csv_bytes",
                "DIRBYTES": "dir_bytes", "DIRFILES": "dir_files"}


def measure_source(host: str, csv_name: str, result_dir: str | None) -> dict:
    """Size of what sits on a host, without transferring any of it."""
    csv_path = f"experiments/{csv_name}"
    dir_path = f"experiments/{result_dir}" if result_dir else "/nonexistent"
    script = (MEASURE_SCRIPT
              .replace("@ROOT@", shlex.quote(source_path(host)))
              .replace("@CSV@", shlex.quote(csv_path))
              .replace("@DIR@", shlex.quote(dir_path))
              .replace("@M@", MARK))
    text, err = run_shell(host, script, timeout=MEASURE_TIMEOUT_S)
    out: dict = {"host": host, "error": err, "timed_out": is_timeout(err)}
    for line in (text or "").splitlines():
        if not line.startswith(MARK):
            continue
        tag, _, rest = line[len(MARK):].partition(" ")
        rest = rest.strip()
        if tag == "ERR":
            out["error"] = rest
        elif tag in MEASURE_KEYS:
            out[MEASURE_KEYS[tag]] = int(rest) if rest.isdigit() else None
    if out.get("csv_rows"):
        out["csv_rows"] -= 1  # header
    return out


def rsync_pending(host: str, csv_name: str, result_dir: str | None) -> dict:
    """What an rsync would actually move, via ``rsync -a --dry-run --stats``.

    Read-only on both ends: rsync builds its file list and reports on it, and
    writes nothing. That is the whole point of collect's dry run against a
    29GB tree.
    """
    out: dict = {}
    root = source_root(host)
    targets = [("csv", f"{root}/experiments/{csv_name}",
                str(REPO_ROOT / "experiments" / csv_name))]
    if result_dir:
        local_dir = REPO_ROOT / "experiments" / result_dir
        targets.insert(0, ("results", f"{root}/experiments/{result_dir}/",
                           f"{local_dir}/"))
    for label, src, dst in targets:
        proc = subprocess.run(
            ["rsync", "-a", "--dry-run", "--stats",
             "-e", " ".join(["ssh", *SSH_OPTS]), src, dst],
            capture_output=True, text=True)
        stats: dict = {"ok": proc.returncode == 0}
        for line in proc.stdout.splitlines():
            if line.startswith("Number of regular files transferred:"):
                stats["files"] = int(line.split(":")[1].strip().replace(",", ""))
            elif line.startswith("Total transferred file size:"):
                stats["bytes"] = int(
                    line.split(":")[1].split()[0].replace(",", ""))
        if not stats["ok"]:
            tail = [ln.strip() for ln in proc.stderr.splitlines() if ln.strip()]
            stats["error"] = tail[-1] if tail else f"exit {proc.returncode}"
        out[label] = stats
    return out


def key_set(path: Path) -> set:
    _, rows = merge.read_rows(path)
    return {merge.key_of(r) for r in rows}


def duplicate_keys(path: Path) -> list:
    counts: dict = {}
    _, rows = merge.read_rows(path)
    for row in rows:
        k = merge.key_of(row)
        counts[k] = counts.get(k, 0) + 1
    return sorted(k for k, n in counts.items() if n > 1)


def verify_merge(results_csv: Path, before: set, per_host: dict,
                 missing: list | tuple = ()) -> bool:
    """Row count, duplicate keys, and per-host counts against the merged total.

    The arithmetic that matters is the union, not the sum. Two hosts running
    disjoint seed ranges overlap on nothing, while re-collecting an already
    merged campaign overlaps on everything; only the union tells those apart
    from a silent loss.

    ``missing`` names hosts that were asked for and contributed nothing. The
    union is computed over the survivors alone and so agrees with itself
    perfectly, which is exactly why a host dropping out has to be carried in
    here rather than inferred: a half-collected campaign otherwise verifies
    clean and exits zero.
    """
    _, merged_rows = merge.read_rows(results_csv)
    merged_keys = {merge.key_of(r) for r in merged_rows}
    expected = set(before)
    for keys in per_host.values():
        expected |= keys
    ok = not missing

    print("\nVerification")
    if missing:
        print(f"  INCOMPLETE: no rows from {', '.join(missing)} — the merged "
              f"file is missing that host's share of the campaign.")
        print("  Everything below is over the hosts that did answer, so it "
              "cannot detect that.")
    print(f"  merged rows:    {len(merged_rows)}")
    print(f"  distinct keys:  {len(merged_keys)}")
    dups = duplicate_keys(results_csv)
    if dups:
        ok = False
        print(f"  DUPLICATE KEYS: {len(dups)} (e.g. {dups[0]})")
    else:
        print("  duplicate keys: none")

    print(f"  local before:   {len(before)}")
    for host, keys in per_host.items():
        print(f"  {host}: {len(keys)} rows ({len(keys & before)} already local)")
    pairs = list(per_host.items())
    for i, (h1, k1) in enumerate(pairs):
        for h2, k2 in pairs[i + 1:]:
            if k1 & k2:
                print(f"  overlap {h1}/{h2}: {len(k1 & k2)} shared keys")
    print(f"  expected union: {len(expected)}")

    if len(merged_rows) != len(merged_keys):
        ok = False
        print("  MISMATCH: the merged file holds more rows than distinct keys")
    if merged_keys != expected:
        ok = False
        absent = expected - merged_keys
        extra = merged_keys - expected
        print(f"  MISMATCH: {len(absent)} expected key(s) absent, "
              f"{len(extra)} unexpected key(s) present")
        for k in sorted(absent)[:5]:
            print(f"    missing: {k}")
        for k in sorted(extra)[:5]:
            print(f"    extra:   {k}")
    if ok:
        print("  OK: every source row is present exactly once.")
    return ok


def collect_dry_run(hosts: list[str], csv_name: str, result_dir: str | None,
                    results_csv: Path) -> int:
    print(f"Local {csv_name}: {len(key_set(results_csv))} rows")
    rows = []
    ok = True
    for host in hosts:
        measured = measure_source(host, csv_name, result_dir)
        if measured.get("error"):
            ok = False
            # A deadline and a dead host need different words: the first says
            # the probe was too cheap for the tree, the second that nobody is
            # home, and only the second means the collect cannot proceed.
            why = ("measure timed out" if measured["timed_out"]
                   else f"unreachable: {measured['error']}")
            rows.append([host, f"({why})", "-", "-", "-", "-"])
            continue
        pending = rsync_pending(host, csv_name, result_dir)
        results = pending.get("results", {})
        csv_stat = pending.get("csv", {})
        for stat in (results, csv_stat):
            if not stat.get("ok", True):
                ok = False
                print(f"  rsync probe failed on {host}: {stat.get('error')}")
        rows.append([
            host,
            str(measured.get("csv_rows", "-")),
            human_bytes(measured.get("dir_bytes")),
            str(measured.get("dir_files", "-")),
            (f"{results.get('files', 0)} files / "
             f"{human_bytes(results.get('bytes'))}") if result_dir else "(skipped)",
            f"{csv_stat.get('files', 0)} files / "
            f"{human_bytes(csv_stat.get('bytes'))}",
        ])
    print()
    print(render_table(rows, ["HOST", "ROWS", "DIR SIZE", "DIR FILES",
                              "WOULD TRANSFER (results)",
                              "WOULD TRANSFER (csv)"]))
    print("\nDry run — nothing transferred, nothing written.")
    return 0 if ok else 1


def cmd_collect(args: argparse.Namespace) -> int:
    csv_name, result_dir = resolve_profile(args)
    hosts = [args.host] if args.host else list(HOSTS)
    results_csv = REPO_ROOT / "experiments" / csv_name
    # Named for what actually chose the file: --csv bypasses --profile
    # entirely, and a banner reading "Profile: full" over an arbiter-probe
    # collect describes a run that is not happening.
    source = (f"{csv_name} (--csv override, --profile unused)" if args.csv
              else f"{args.profile} → {csv_name}")
    print(f"Collecting: {source}"
          + (f" + experiments/{result_dir}/" if result_dir else " (CSV only)"))
    print(f"Hosts:      {', '.join(hosts)}\n")

    if args.dry_run:
        return collect_dry_run(hosts, csv_name, result_dir, results_csv)

    before = key_set(results_csv)
    tmp_dir = REPO_ROOT / "experiments" / ".merge_tmp"
    tmp_dir.mkdir(parents=True, exist_ok=True)
    pulled: dict = {}
    missing: list[str] = []
    for host in hosts:
        # merge_experiments does the transfer and the merge; only the ssh
        # destination differs, so pull_source is called directly rather than
        # through resolve_source, whose REMOTES entry is the FQDN form.
        # It wraps only the CSV rsync against failure, so the per-run transfer
        # raises out of here on an unreachable host and would otherwise lose
        # the reachable host's rows to a traceback.
        try:
            path = merge.pull_source(host, source_root(host),
                                     False, tmp_dir, csv_name, result_dir)
        except subprocess.CalledProcessError as exc:
            print(f"  ERROR: {host} transfer failed (rsync exit "
                  f"{exc.returncode})")
            missing.append(host)
            continue
        if path is None:
            print(f"  ERROR: {host} contributed no {csv_name}")
            missing.append(host)
            continue
        pulled[host] = path

    if not pulled:
        print(f"\nNo {csv_name} found on any host — nothing merged.")
        return 1
    per_host = {host: key_set(path) for host, path in pulled.items()}
    # Merge what did arrive: the merge is idempotent and keyed, so a later run
    # against the recovered host completes the file rather than redoing it.
    # The exit status still fails, because a partial collect that reports
    # success is the silent loss this verification exists to catch.
    merge.merge_csv(list(pulled.values()), results_csv)
    return 0 if verify_merge(results_csv, before, per_host, missing) else 1


# -- Entry point --------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="verb", required=True)

    status = sub.add_parser("status", help="Live campaign state, per host.")
    status.add_argument("--host", choices=[*HOSTS, LOCAL],
                        help="Only this host (default: every host plus the "
                             "local checkout).")
    status.add_argument("--campaign", metavar="PROFILE",
                        help="Only this profile's campaign.")
    status.add_argument("--all", action="store_true",
                        help="Include manifests left by campaigns launched "
                             "from a branch the checkout has since left.")
    status.add_argument("--json", action="store_true",
                        help="Machine-readable output.")
    status.add_argument("--no-plan", action="store_true",
                        help="Skip the per-campaign --dry-run plan query; "
                             "planned totals then read as unknown.")
    status.set_defaults(func=cmd_status)

    collect = sub.add_parser(
        "collect", help="rsync each host's results back, merge and verify.")
    collect.add_argument("--profile", default="full",
                         help="Profile whose CSV to collect (default: full).")
    collect.add_argument("--host", choices=list(HOSTS),
                         help="Only this host (default: every host).")
    collect.add_argument("--dry-run", action="store_true",
                         help="Report what would be transferred; transfer "
                              "nothing and write nothing.")
    collect.add_argument("--no-results", action="store_true",
                         help="Merge the CSV only; skip the per-run trees.")
    collect.add_argument("--csv", metavar="NAME",
                         help="Results CSV name, for a campaign whose profile "
                              "this checkout does not define.")
    collect.add_argument("--results-dir", metavar="NAME",
                         help="Per-run directory name, alongside --csv.")
    collect.set_defaults(func=cmd_collect)
    return parser


def main() -> None:
    args = build_parser().parse_args()
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
