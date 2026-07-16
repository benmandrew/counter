#!/usr/bin/env python3
"""Round-trip tests for the factor-directory parsing in run_experiments.py.

No pytest dependency: run it directly (``python scripts/test_experiment_paths.py``)
and it exits non-zero on the first failure. The one thing worth guarding is that
``scheme_of`` / ``weakening_of`` / ``metric_of`` recover the right factor from a
path regardless of how deep the factors nest — a config that lands one level
deeper than a parser expects is silently mis-attributed to the wrong cell and
its rows key wrong (the trap REPORT.md flags).
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import run_experiments as R  # noqa: E402
import gen_configs as G  # noqa: E402


def check(got, want, msg):
    if got != want:
        print(f"FAIL: {msg}\n  got:  {got!r}\n  want: {want!r}")
        sys.exit(1)


def factors(path_str):
    p = Path(path_str)
    return R.scheme_of(p), R.weakening_of(p), R.metric_of(p)


BASE = "experiments/configs/{}/sweep_C_default.toml"

# (layout, expected (scheme, weakening, metric)). LEGACY_WEAKENING/METRIC fill
# in for any factor segment the layout omits.
CASES = [
    # flat: predates both factors
    ("nsga2", ("nsga2", R.LEGACY_WEAKENING, R.LEGACY_METRIC)),
    # weakening only
    ("nsga2/wkon", ("nsga2", "wkon", R.LEGACY_METRIC)),
    ("weighted/wkoff", ("weighted", "wkoff", R.LEGACY_METRIC)),
    # metric only (weakening absent)
    ("nsga2/direct", ("nsga2", R.LEGACY_WEAKENING, "direct")),
    ("nsga2/log", ("nsga2", R.LEGACY_WEAKENING, "log")),
    # three deep: both factors, canonical <scheme>/<weakening>/<metric> order
    ("nsga2/wkon/direct", ("nsga2", "wkon", "direct")),
    ("nsga2/wkoff/log", ("nsga2", "wkoff", "log")),
    ("weighted/wkon/log", ("weighted", "wkon", "log")),
]

for layout, want in CASES:
    check(factors(BASE.format(layout)), want, f"factors of <{layout}>")

# The scan is order-independent, so a swapped nesting still resolves — the
# guarantee the ancestor-scan buys over a fixed parent/parent.parent walk.
check(factors(BASE.format("nsga2/log/wkoff")), ("nsga2", "wkoff", "log"),
      "swapped metric/weakening nesting")

# extract_metadata is unaffected by the extra depth.
p = Path(BASE.format("nsga2/wkon/direct"))
check(R.extract_metadata(p)[:2], ("C", "default"), "extract_metadata three-deep")

# gen_configs METRICS maps the short dir label to the full TOML value the C++
# parser accepts; metric_of must key on the label, not the value.
check([lbl for lbl, _ in G.METRICS["both"]], ["direct", "log"],
      "METRICS both dir labels")
check([val for _, val in G.METRICS["both"]], ["direct", "logarithmic"],
      "METRICS both toml values")
for lbl, _ in G.METRICS["both"]:
    assert lbl in R.METRIC_DIRS, f"{lbl} not in METRIC_DIRS"

# The merge key must carry every crossed factor, or crossed rows collapse.
import merge_experiments as M  # noqa: E402
for f in ("selection", "weakening", "metric"):
    assert f in M.KEY_FIELDS, f"{f} missing from merge KEY_FIELDS"
    assert f in R.CSV_FIELDS, f"{f} missing from CSV_FIELDS"

print("ok: all factor-path round-trips pass")
