"""Post-hoc triviality audit for the 2026-08-07 elitism campaign (PLAN.md section 5).

Criterion 3 asks for the fraction of written repairs whose guarantee side
reduces to `true`. Two readings of that are reported, because they disagree and
only the second answers the objection the criterion was written for:

  whole      every guarantee-side conjunct is valid, so the specification
             constrains nothing at all. Zero in both arms on both paths.
  per-guarantee
             at least one conjunct is valid, so that requirement was deleted
             outright while the others stand. This is the lily02 shape, and it
             is the number the campaign reports.

The guarantee side is a conjunction, so it is valid iff every conjunct is, and
a conjunct is tested on its own. TLSF ASSERT and PRESET conjuncts are tested
unwrapped, since `G psi` is valid exactly when `psi` is -- the same identity
tlsf_has_valid_guarantee relies on.

Formulae come from the `ltl` binary rather than a second lowering written here,
so the audit reads what the engine produced. Run it against a commit whose
src/ and include/ match the campaign's.

Usage: python3 triviality_audit.py <results-dir> [<results-dir> ...]
"""

import collections
import glob
import os
import re
import subprocess
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

LTL = os.environ.get("COUNTER_LTL", "build-release/ltl")
LTLFILT = os.environ.get("LTLFILT", "ltlfilt")
GUARANTEE_SECTIONS = ("PRESET", "ASSERT", "GUARANTEE")

_memo = {}
_memo_lock = threading.Lock()


def guarantee_side(path):
    """The guarantee-side conjuncts of one written repair, as the engine lowers them."""
    out = subprocess.run([LTL, path], capture_output=True, text=True, timeout=300)
    if out.returncode != 0:
        raise RuntimeError(f"ltl failed on {path}: {out.stderr.strip()[:200]}")
    formulae = []
    if path.endswith(".tlsf"):
        section = None
        for line in out.stdout.splitlines():
            header = re.match(r"^  ([A-Z]+):$", line)
            if header:
                section = header.group(1)
            elif line.startswith("  combined LTL:"):
                section = None
            elif section in GUARANTEE_SECTIONS and line.startswith("    "):
                formulae.append(line.strip())
    else:
        tag = None
        for line in out.stdout.splitlines():
            if "[guarantee]" in line:
                tag = "guarantee"
            elif "[assumption]" in line:
                tag = "assumption"
            elif line.strip().startswith("LTL:") and tag == "guarantee":
                formulae.append(line.strip()[len("LTL:"):].strip())
    return formulae


def is_valid(formula):
    """Whether the formula is a tautology. Returns True, False, or "timeout"."""
    with _memo_lock:
        if formula in _memo:
            return _memo[formula]
    try:
        out = subprocess.run([LTLFILT, "-f", formula, "--equivalent-to=1"],
                             capture_output=True, text=True, timeout=120)
        verdict = "timeout" if out.returncode not in (0, 1) else bool(out.stdout.strip())
    except subprocess.TimeoutExpired:
        verdict = "timeout"
    with _memo_lock:
        _memo[formula] = verdict
    return verdict


def audit_repair(path):
    formulae = guarantee_side(path)
    valid = [f for f in formulae if is_valid(f) is True]
    timeouts = [f for f in formulae if is_valid(f) == "timeout"]
    return path, len(formulae), valid, timeouts


def arm_of(run_dir):
    return "elit0.1" if "_elit0.1_" in os.path.basename(run_dir) else "elit0"


def main(roots):
    files = []
    for root in roots:
        for pattern in ("repair_*.json", "repair_*.tlsf"):
            files += sorted(glob.glob(os.path.join(root, "*", pattern)))
    files = [f for f in files if not f.endswith(".fitness.json")]
    print(f"auditing {len(files)} written repairs under {roots}")

    repairs = collections.Counter()
    per_file = collections.Counter()
    per_run = collections.defaultdict(set)
    whole = collections.Counter()
    stalled = collections.Counter()
    hits = []
    with ThreadPoolExecutor(max_workers=12) as pool:
        for path, n, valid, timeouts in pool.map(audit_repair, files):
            run_dir = os.path.dirname(path)
            arm = arm_of(run_dir)
            repairs[arm] += 1
            stalled[arm] += len(timeouts)
            if valid:
                per_file[arm] += 1
                per_run[arm].add(run_dir)
                hits.append((arm, path, len(valid), n, valid[0]))
            if n and len(valid) == n:
                whole[arm] += 1

    print("\narm       repairs  per-guarantee (files/runs)  whole side  ltlfilt timeouts")
    for arm in sorted(repairs):
        print(f"{arm:8s}  {repairs[arm]:7d}  {per_file[arm]:6d}/{len(per_run[arm]):-4d}"
              f"{'':16s}{whole[arm]:2d}{'':10s}{stalled[arm]}")
    for arm, path, k, n, formula in sorted(hits):
        print(f"\n  {arm}  {path}\n    {k}/{n} conjuncts valid: {formula}")
    if not hits:
        print("\nno tautologous guarantee under either reading")


if __name__ == "__main__":
    main(sys.argv[1:])
