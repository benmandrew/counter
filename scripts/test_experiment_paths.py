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


def repair(path_str):
    return R.repair_mode_of(Path(path_str))


# repair_mode nests deepest; its presence must not disturb the other factors,
# and scheme_of must still skip a repair segment to find the scheme.
REPAIR_CASES = [
    ("nsga2", R.LEGACY_REPAIR),                     # flat: predates the factor
    ("nsga2/mono", "mono"),
    ("nsga2/muc", "muc"),
    ("nsga2/wkon/direct/muc", "muc"),               # four deep, canonical order
    ("nsga2/muc/wkon", "muc"),                       # order-independent scan
]
for layout, want in REPAIR_CASES:
    check(repair(BASE.format(layout)), want, f"repair_mode of <{layout}>")

# A repair segment must not be mistaken for the scheme or any other factor.
check(factors(BASE.format("nsga2/wkon/direct/muc")),
      ("nsga2", "wkon", "direct"), "factors ignore the repair segment")
check(repair(BASE.format("nsga2/wkon/direct")), R.LEGACY_REPAIR,
      "repair_mode falls back to LEGACY_REPAIR when absent")

# gen_configs REPAIRS maps the short dir label to the TOML value the C++ parser
# accepts; repair_mode_of must key on the label, not the value.
check([lbl for lbl, _ in G.REPAIRS["both"]], ["mono", "muc"],
      "REPAIRS both dir labels")
check([val for _, val in G.REPAIRS["both"]], ["monolithic", "muc"],
      "REPAIRS both toml values")
for lbl, _ in G.REPAIRS["both"]:
    assert lbl in R.REPAIR_DIRS, f"{lbl} not in REPAIR_DIRS"

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
for f in ("selection", "weakening", "metric", "repair_mode"):
    assert f in M.KEY_FIELDS, f"{f} missing from merge KEY_FIELDS"
    assert f in R.CSV_FIELDS, f"{f} missing from CSV_FIELDS"


# ── Ablation-campaign profile invariants ─────────────────────────────────────

P = R.PROFILES

# Extending TLSF_SPECS for the ablation campaign must not grow the corpora of
# the profiles whose results CSVs are already recorded against the original
# six families.
check(P["tlsf"]["specs"], R.TLSF_CORE_SPECS, "tlsf profile corpus")
check(P["muc"]["specs"], R.TLSF_CORE_SPECS, "muc profile corpus")
for name in ("padd", "wellsep"):
    check(P[name]["specs"],
          [s for s in R.TLSF_CORE_SPECS if s != "humanoid-531"],
          f"{name} profile corpus")

# The two ablation arms are the same 8-cell factorial: 2 schemes x 2 metrics x
# 2 sweep-C levels (default = Halstead 0.1, no-halstead = 0.0).
check(len(R.TLSF_ABLATION_SPECS), 13, "ablation TLSF corpus size")
for name in ("ablate-fret", "ablate-tlsf"):
    check(sorted(P[name]["schemes"]), ["nsga2", "weighted"],
          f"{name} schemes")
    check(P[name]["metrics"], ["direct", "log"], f"{name} metrics")
    check(P[name]["levels"], {"C": ["default", "no-halstead"]},
          f"{name} sweep-C levels")
    check(P[name]["baseline_aliases"], {}, f"{name} has no aliases")
check(P["ablate-tlsf"]["specs"], R.TLSF_ABLATION_SPECS, "ablate-tlsf corpus")

# h2h-tlsf dedups against ablate-tlsf by sharing its configs dir, results dir
# and results CSV: the resume key carries every factor, so the control-cell
# rows ablate-tlsf completed are skipped rather than re-run. If any of these
# three diverge the dedup silently breaks and the top-up re-runs 195 rows.
for field in ("configs_dir", "results_dir", "results_csv"):
    check(P["h2h-tlsf"][field], P["ablate-tlsf"][field],
          f"h2h-tlsf shares ablate-tlsf {field}")
# ... and it must stay inside the control cell, with the metric crossed (not
# None/legacy) so its run_id and CSV metric column match ablate-tlsf's log cell.
check(P["h2h-tlsf"]["schemes"], ["nsga2"], "h2h-tlsf control scheme")
check(P["h2h-tlsf"]["metrics"], ["log"], "h2h-tlsf control metric")
check(P["h2h-tlsf"]["levels"], {"C": ["default"]}, "h2h-tlsf control level")
check(P["h2h-tlsf"]["specs"], R.H2H_TLSF_SPECS, "h2h-tlsf corpus")

# The head-to-head corpus is the 11 AuRUS-matched ablation families plus the
# two AuRUS imports. counter's own arbiter (a different problem from AuRUS's,
# see EXPERIMENTS.md 2026-07-24) and amba (no AuRUS case study) stay out.
check(len(R.H2H_TLSF_SPECS), 13, "h2h TLSF corpus size")
for excluded in ("arbiter", "amba"):
    assert excluded not in R.H2H_TLSF_SPECS, \
        f"{excluded} must not be in the head-to-head corpus"
for imported in ("takeoff-tlsf", "arbiter-aurus"):
    assert imported in R.H2H_TLSF_SPECS, \
        f"{imported} missing from the head-to-head corpus"

# aurus_campaign runs AuRUS on exactly the head-to-head corpus, keyed by
# counter family name so its CSV joins against the h2h-tlsf rows.
import aurus_campaign as A  # noqa: E402
check(sorted(A.SPEC_TLSF), sorted(R.H2H_TLSF_SPECS),
      "aurus_campaign covers the head-to-head corpus")
check(A.SPEC_TLSF["arbiter-aurus"], "arbiter/arbiter.tlsf",
      "arbiter-aurus maps to AuRUS's arbiter case study")
check(A.SPEC_TLSF["takeoff-tlsf"], "takeoff/takeoff.tlsf",
      "takeoff-tlsf maps to AuRUS's takeoff case study")

# merge_experiments mirrors the per-profile CSV names; the ablation profiles
# must be present there, with h2h-tlsf pointing at ablate-tlsf's CSV.
for name in ("ablate-fret", "ablate-tlsf", "h2h-tlsf"):
    check(M.PROFILE_CSVS[name], P[name]["results_csv"].name,
          f"merge CSV for {name}")
    check(M.PROFILE_RESULT_DIRS[name], P[name]["results_dir"].name,
          f"merge result dir for {name}")

# Every spec a profile names must resolve to an on-disk input, and every
# capped profile must cap every spec it runs (run_one indexes caps[spec]).
for name, prof in P.items():
    for s in prof["specs"]:
        assert R.SPECS[s]["input"].exists(), \
            f"{name}: missing input for spec {s}"
        if prof["timeout_caps"] is not None:
            assert s in prof["timeout_caps"], f"{name}: no timeout cap for {s}"

print("ok: all factor-path round-trips pass")
