#!/usr/bin/env python3
"""Run parameter sweep experiments and collect metrics to a results CSV.

A profile selects which (sweep, level, spec, seed) combinations run and which
CSV they land in (see scripts/README.md):
    full     — every level, all 4 specs, seeds 0-29 → experiments/results.csv
    cj-large — sweeps C/D/E/F/I, nsga2 only, run_weakening crossed on/off, all
               4 specs, seeds 0-89, at generations=40/population_size=1000 →
               experiments/results-cj-large.csv

Usage:
    python scripts/run_experiments.py                    # the full sweep
    python scripts/run_experiments.py --profile full     # the same, explicitly
    python scripts/run_experiments.py --jobs 8           # 8 parallel runs
    python scripts/run_experiments.py --sweeps A B       # specific sweeps
    python scripts/run_experiments.py --specs takeoff    # specific specs
    python scripts/run_experiments.py --seeds 0 1 2      # specific seeds
    python scripts/run_experiments.py --dry-run          # print plan, no execution
    python scripts/run_experiments.py --no-resume        # ignore existing results

Every row records the commit the counter binary was built from, read once at
startup from `counter --version` rather than from the working tree — the two
diverge whenever a fix lands without a rebuild. A launch whose binary does not
match HEAD, or was built dirty, is refused; --allow-stale-binary downgrades
that to a warning. A per-host manifest lands beside the results CSV with the
branch, describe, binary commits and the sweep the launch asked for.
"""

import argparse
import csv
import json
import math
import os
import re
import socket
import subprocess
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
EXPERIMENTS_DIR = REPO_ROOT / "experiments"
CONFIGS_DIR = EXPERIMENTS_DIR / "configs"
RESULTS_DIR = EXPERIMENTS_DIR / "results"

# Overridable so a checkout without a release build (e.g. a worktree) can
# point at another checkout's binaries.
COUNTER_BIN = Path(os.environ.get("COUNTER_BIN",
                                  REPO_ROOT / "build-release" / "counter"))
COMPARE_BIN = Path(os.environ.get("COMPARE_BIN",
                                  REPO_ROOT / "build-release" / "compare"))

EXAMPLES_DIR = REPO_ROOT / "examples"


def _spec(name: str, ext: str) -> dict[str, Path]:
    return {"input": EXAMPLES_DIR / name / f"spec.{ext}",
            "ideals_dir": EXAMPLES_DIR / name / "fixes"}


# FRETISH (JSON) specs: the original repair benchmark. repair_mode is TLSF-only
# and does not affect these, so the mono-vs-muc factor is never crossed over
# them — the "muc" profile uses the TLSF specs below.
FRETISH_SPECS: dict[str, dict[str, Path]] = {
    "takeoff": _spec("takeoff", "json"),
    "fsm": _spec("fsm", "json"),
    "fsm-timing": _spec("fsm-timing", "json"),
    "fsm-combined": _spec("fsm-combined", "json"),
}

# Basic-TLSF specs with ideal fixes. counter infers the TLSF format from the
# .tlsf extension, and compare reads the .tlsf ideals the same way. The first
# six are the original mono-vs-muc corpus; the rest were imported from the
# aurus tree (9e5fc08 and the takeoff-tlsf/arbiter-aurus follow-ups) for the
# ablation campaign. takeoff-tlsf is the AuRUS takeoff case study as TLSF — a
# separate family from the FRETISH examples/takeoff/ that FRETISH_SPECS names.
# It is inert: no profile runs it, since both upstream "genuine" fixes proved
# invalid (see EXPERIMENTS.md 2026-07-24), leaving it without ideals.
TLSF_SPECS: dict[str, dict[str, Path]] = {
    "arbiter": _spec("arbiter", "tlsf"),
    "gyro-var1": _spec("gyro-var1", "tlsf"),
    "humanoid-531": _spec("humanoid-531", "tlsf"),
    "lift": _spec("lift", "tlsf"),
    "lily02": _spec("lily02", "tlsf"),
    "minepump": _spec("minepump", "tlsf"),
    "gyro-var2": _spec("gyro-var2", "tlsf"),
    "humanoid-458": _spec("humanoid-458", "tlsf"),
    "amba": _spec("amba", "tlsf"),
    "codesample-un1": _spec("codesample-un1", "tlsf"),
    "codesample-un2": _spec("codesample-un2", "tlsf"),
    "rg1": _spec("rg1", "tlsf"),
    "rg2": _spec("rg2", "tlsf"),
    "takeoff-tlsf": _spec("takeoff-tlsf", "tlsf"),
    "arbiter-aurus": _spec("arbiter-aurus", "tlsf"),
    # SYNTCOMP arbiter family, imported without ideals in 9e5fc08 and promoted
    # into the ablation corpus on 2026-07-24 when hand-written ideal repairs
    # landed (system-weakening fixes, realize-validated; see EXPERIMENTS.md).
    "arbiter-handshake": _spec("arbiter-handshake", "tlsf"),
    "detector": _spec("detector", "tlsf"),
    "full-arbiter": _spec("full-arbiter", "tlsf"),
    "load-balancer": _spec("load-balancer", "tlsf"),
    "prioritized-arbiter": _spec("prioritized-arbiter", "tlsf"),
    "round-robin-arbiter": _spec("round-robin-arbiter", "tlsf"),
    "simple-arbiter": _spec("simple-arbiter", "tlsf"),
}

# The original six-family TLSF corpus. The pre-ablation TLSF profiles (tlsf,
# muc, padd, wellsep) pin this list rather than list(TLSF_SPECS), so extending
# the table above does not silently grow what they run — their results CSVs
# are already recorded against exactly these six.
TLSF_CORE_SPECS: list[str] = [
    "arbiter", "gyro-var1", "humanoid-531", "lift", "lily02", "minepump",
]

# The 20 fixes-backed TLSF families of the ablation campaign (PLAN §2): the
# original 13, plus the 7 SYNTCOMP arbiter families promoted on 2026-07-24
# after hand-written ideals landed for them.
TLSF_ABLATION_SPECS: list[str] = [
    "arbiter", "gyro-var1", "gyro-var2", "humanoid-458", "humanoid-531",
    "lift", "lily02", "minepump", "amba", "codesample-un1", "codesample-un2",
    "rg1", "rg2",
    "arbiter-handshake", "detector", "full-arbiter", "load-balancer",
    "prioritized-arbiter", "round-robin-arbiter", "simple-arbiter",
]

# The 12-family AuRUS head-to-head corpus: the 11 ablation families with an
# AuRUS case-studies match, plus arbiter-aurus (imported from the AuRUS tree
# so both tools solve the same spec — counter's own arbiter is a hand-written
# GR(1) mutex, a different problem from AuRUS's request-response arbiter, and
# stays in the ablation corpus only). amba has no AuRUS case study.
# takeoff-tlsf was imported for the head-to-head but is excluded: both of its
# upstream "genuine" fixes are invalid (one truncated, one unsatisfiable), so
# the family has no ideals to score implies_ideal against — see EXPERIMENTS.md
# 2026-07-24. The list is explicit rather than derived from
# TLSF_ABLATION_SPECS so growing the ablation corpus (e.g. the 2026-07-24
# SYNTCOMP promotion, whose ideals are hand-written, not AuRUS-genuine) cannot
# silently widen the head-to-head.
H2H_TLSF_SPECS: list[str] = [
    "gyro-var1", "gyro-var2", "humanoid-458", "humanoid-531", "lift",
    "lily02", "minepump", "codesample-un1", "codesample-un2", "rg1", "rg2",
    "arbiter-aurus",
]

# The arbitration corpus of the 2026-08-10 diversity probe. 2026-07-31-replicate
# found nsga2-apportion (then nsga2-replicate) repairing `arbiter` 120/120 where
# nsga2-truncate managed 0/120 — the only place either NSGA-II scheme has ever
# separated on found_repair, and the lever the arbiter-hp and arbiter-padd
# campaigns hunted through p_add_assumption without finding. That result rests on
# one family, so this list is every fixes-backed family whose unrealizability is
# an arbitration conflict: the seven arbiters, plus amba (a bus arbiter) and
# load-balancer (arbitration over servers). Six of the nine did not exist in the
# corpus when replicate ran.
#
# lily02 is the tenth and is not an arbiter. It is the counter-signal control:
# replicate's apportion arm cost it implies_ideal 1.00 -> 0.44, so a probe that
# only sampled families the scheme helps would report the upside alone.
ARBITER_PROBE_SPECS: list[str] = [
    "arbiter", "arbiter-aurus", "arbiter-handshake", "full-arbiter",
    "prioritized-arbiter", "round-robin-arbiter", "simple-arbiter",
    "amba", "load-balancer",
    "lily02",
]

# Unified lookup for run_one()/--specs; each profile picks its own subset.
SPECS: dict[str, dict[str, Path]] = {**FRETISH_SPECS, **TLSF_SPECS}

N_SEEDS = 30

# compare runs n_repairs * n_ideals * 2 implication checks, each with compare's
# own 20s black budget. fsm-timing (bounded-interval operators) is slow: a 29-
# repair run measured ~141s, and repair counts grow with generations/population.
# Keep this well above that, in line with the generous counter_timeout budgets.
# A profile whose arms differ in repair count should raise it via its own
# `compare_timeout` key -- see the compare_timed_out column below.
COMPARE_TIMEOUT_S = 600

CSV_FIELDS = [
    "sweep", "level_name", "level_value", "selection", "weakening", "metric",
    "repair_mode", "spec", "seed", "found_repair", "n_repairs", "best_fitness",
    "best_relation", "implies_ideal", "n_implies", "wall_time_s",
    "timed_out", "n_dropped", "compare_timed_out", "commit", "dirty",
]

# Rows written before `selection` existed are all NSGA-II: every config in use
# pinned selection_scheme = "nsga2", verified against the per-run config.toml
# snapshots of the 2026-07-14 sweep (1470/1470). Defaulting them to "nsga2"
# keeps resume working against those CSVs instead of re-running them.
LEGACY_SELECTION = "nsga2"

# The two NSGA-II schemes were renamed -- "nsga2" -> "nsga2-truncate" and
# "nsga2-replicate" -> "nsga2-apportion" -- and the binary rejects the old
# spellings outright. Only the name moved: the scheme each one selects is
# unchanged, so a row recorded under either spelling names the same cell of the
# design. 224,861 archived rows carry "nsga2" and the replicate campaign's rows
# carry "nsga2-replicate", against zero rows written under the new names so far.
# Canonicalising every `selection` value as it is read back -- from an archived
# CSV on resume, or from a config directory name -- is what keeps those rows
# matching the plan instead of missing it and re-running finished campaigns.
# gen_configs.py emits the new names only; this mapping is read-side alone.
SCHEME_ALIASES: dict[str, str] = {
    "nsga2": "nsga2-truncate",
    "nsga2-replicate": "nsga2-apportion",
}


def canonical_scheme(name: str) -> str:
    """Current spelling of a selection scheme; any other name passes through."""
    return SCHEME_ALIASES.get(name, name)


# Rows written before `weakening` existed ran configs with run_weakening = true
# (the DEFAULTS value, overridden only by sweep J's weaken-off level, which the
# profiles of that era did not run). Same purpose as LEGACY_SELECTION: keep
# resume matching those rows rather than re-running them.
LEGACY_WEAKENING = "wkon"

# Directory names gen_configs.py --weakening emits. A config directly under
# <scheme>/ predates the factor and is read as LEGACY_WEAKENING.
WEAKENING_DIRS: tuple[str, ...] = ("wkon", "wkoff")

# Rows written before `metric` existed ran the direct similarity metric (the
# config.hpp / DEFAULTS value; model_counting.metric did not yet exist, so the
# binary always scored directly). Same purpose as LEGACY_SELECTION: keep resume
# and merge matching those rows rather than re-running them.
LEGACY_METRIC = "direct"

# Directory names gen_configs.py --metric emits (the short label, not the TOML
# value "logarithmic"). A config with no such ancestor predates the factor and
# is read as LEGACY_METRIC.
METRIC_DIRS: tuple[str, ...] = ("direct", "log")

# Rows written before `repair_mode` existed ran monolithic repair (the
# config.hpp / DEFAULTS value; tlsf.repair_mode did not yet exist, so the binary
# always evolved the whole spec). Same purpose as LEGACY_SELECTION: keep resume
# and merge matching those rows rather than re-running them.
LEGACY_REPAIR = "mono"

# Directory names gen_configs.py --repair emits (the short label, not the TOML
# value "monolithic"). A config with no such ancestor predates the factor and is
# read as LEGACY_REPAIR.
REPAIR_DIRS: tuple[str, ...] = ("mono", "muc")

# Rows written before `commit` existed have no recorded commit at all: nothing
# asked the binary what it was built from, and a binary's commit is not
# recoverable after the fact (mtime bounds a window, and the binaries on av2/av3
# lag main by an unbounded amount — see the PROVENANCE.json files under
# experiments/). Unlike LEGACY_SELECTION this is an admission of ignorance
# rather than a known value, so it must never join the resume or merge key:
# keying on it would make every archived row miss and re-run whole campaigns.
LEGACY_COMMIT = "unknown"

# Every factor directory name, so scheme_of can tell a scheme segment apart
# from a factor segment regardless of nesting order or depth.
FACTOR_DIRS: frozenset[str] = (frozenset(WEAKENING_DIRS) | frozenset(METRIC_DIRS)
                               | frozenset(REPAIR_DIRS))

# Higher index = better relation
RELATION_PRIORITY: dict[str, int] = {
    "equivalent":       4,
    "strictly stronger": 3,
    "strictly weaker":  2,
    "incomparable":     1,
    "timeout":          0,
    "none":            -1,
}

SUMMARY_RE = re.compile(
    r"Summary:\s*(\d+) equivalent,\s*(\d+) strictly stronger,"
    r"\s*(\d+) strictly weaker,\s*(\d+) incomparable,\s*(\d+) timeout"
)
PER_REPAIR_RE = re.compile(
    r"^\S.*?\s+:\s+(equivalent|strictly stronger|strictly weaker|incomparable|timeout)"
)
# counter's scoring report (src/repair/reports.cpp print_scoring_report).
# That report prints unconditionally, unlike the rest of counter's end-of-run
# counters, precisely because this regex reads it out of the log. Individuals
# whose fitness scoring throws are dropped from their generation rather than
# aborting the run, up to Config::max_scoring_failure_rate. The report is
# silent when nothing was dropped, so an absent match means zero — but a run
# that dropped individuals evolved a thinner population than it asked for, and
# without this column that is indistinguishable from a clean run.
DROPPED_RE = re.compile(r"^(\d+) individual\(s\) dropped", re.MULTILINE)


# ── Baseline aliasing ────────────────────────────────────────────────────────

# Each sweep holds every other parameter at its default, so each one's default
# level is byte-identical to the canonical baseline. A canonical run executes at
# most once and emits one CSV row per requested alias — the rows differ only in
# sweep/level_name/level_value.
#
# Aliasing is always within a scheme: nsga2's B/pop200 aliases onto nsga2's
# A/gen10, never onto weighted's. The byte-identity check below enforces that,
# since the configs differ on selection_scheme.
#
# The canonical is per-profile: it must be a level the profile actually runs.
BASELINE_ALIASES: dict[tuple[str, str], tuple[str, str]] = {
    ("B", "pop200"): ("A", "gen10"),
    ("C", "default"): ("A", "gen10"),
    ("D", "ptrig0.5"): ("A", "gen10"),
    ("E", "presp0.5"): ("A", "gen10"),
    ("F", "ptim0.15"): ("A", "gen10"),
    ("G", "bound20"): ("A", "gen10"),
    ("H", "cross0.1"): ("A", "gen10"),
    ("I", "mut1.0"): ("A", "gen10"),
    ("J", "weaken-on"): ("A", "gen10"),
}

# Sweeps A and B are not run at all in the C-J campaign, so its baseline has to
# be C/default — the profile's only remaining all-defaults level. G and H are
# dropped for want of signal, and J is gone entirely: run_weakening is a crossed
# factor of this campaign, not a sweep, so both its states are run against every
# level and the alias holds within a (scheme, weakening) pair.
CJ_LARGE_ALIASES: dict[tuple[str, str], tuple[str, str]] = {
    ("D", "ptrig0.5"): ("C", "default"),
    ("E", "presp0.5"): ("C", "default"),
    ("F", "ptim0.15"): ("C", "default"),
    ("I", "mut1.0"): ("C", "default"),
}

# The TLSF campaign runs only sweeps A (generations) and B (population), which
# cross at the gen10/pop200 baseline, so B/pop200 aliases onto A/gen10 exactly
# as in the FRETISH grid. No other sweep is generated, so the rest of
# BASELINE_ALIASES has nothing to match and is left out.
TLSF_ALIASES: dict[tuple[str, str], tuple[str, str]] = {
    ("B", "pop200"): ("A", "gen10"),
}


# ── Profiles ─────────────────────────────────────────────────────────────────

# Each profile selects a subset of the configs generated by gen_configs.py.
# `schemes` picks which <configs_dir>/<scheme>/ directories are read.
# `weakenings` picks the <scheme>/<wkon|wkoff>/ directories under those; None
# means the profile predates the factor and reads the flat <scheme>/ layout,
# recording every row as LEGACY_WEAKENING.
# `metrics` picks the <direct|log> directories nested below (after weakening,
# where present); None means the flat layout, recorded as LEGACY_METRIC.
# `repair_modes` picks the <mono|muc> directories nested below the metric one;
# None means the flat layout, recorded as LEGACY_REPAIR.
# `sweeps` is the set run by default (--sweeps overrides it, so a profile can
# hold back a sweep until asked). `levels` restricts a sweep to named levels;
# sweeps absent from the map keep every level found in `configs_dir`.
# `timeout_caps` is a flat per-spec cap in seconds; None means the
# counter_timeout() formula.
#
# `configs_dir` and `results_dir` are per-profile because run_id names collide
# across profiles: the same (sweep, level, scheme, spec, seed) is one directory
# name regardless of operating point, so two profiles sharing a results_dir
# would have parse_repair_files() read the other's stale repair_*.json.
PROFILES: dict[str, dict] = {
    # The original sweep, pinned to the levels it has always had so its
    # results.csv stays a single comparable dataset even though gen_configs.py
    # now emits a finer grid around them.
    "full": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["A", "B", "C"],
        "levels": {
            "A": ["gen5", "gen10", "gen20", "gen40"],
            "B": ["pop50", "pop100", "pop200", "pop500", "pop1000"],
        },
        "specs": list(FRETISH_SPECS),
        "seeds": list(range(N_SEEDS)),
        "timeout_caps": None,
        "baseline_aliases": BASELINE_ALIASES,
        "configs_dir": CONFIGS_DIR,
        "results_dir": RESULTS_DIR,
        "results_csv": EXPERIMENTS_DIR / "results.csv",
        "default_jobs": 1,
    },
    # The full factorial: every level of every sweep, run under both selection
    # schemes, at 100 seeds. selection_scheme is a factor here rather than a
    # constant, which is what makes nsga2-vs-weighted answerable per level
    # instead of only at the baseline.
    "factorial": {
        "schemes": ["nsga2-truncate", "weighted"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": None,  # every sweep found in experiments/configs/
        "levels": {},
        "specs": list(FRETISH_SPECS),
        "seeds": list(range(100)),
        "timeout_caps": None,
        "baseline_aliases": BASELINE_ALIASES,
        "configs_dir": CONFIGS_DIR,
        "results_dir": RESULTS_DIR,
        "results_csv": EXPERIMENTS_DIR / "results-factorial.csv",
        "default_jobs": 4,
    },
    # Sweeps C, D, E, F, I at a larger operating point (generations=40,
    # population_size=1000), NSGA-II only, crossed with run_weakening. A and B
    # are excluded because they vary exactly the two parameters this campaign
    # pins, so they would contradict the operating point rather than sweep
    # around it; G and H are excluded for want of signal at the smaller one.
    "cj-large": {
        "schemes": ["nsga2-truncate"],
        "weakenings": ["wkon", "wkoff"],
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["C", "D", "E", "F", "I"],
        "levels": {},
        "specs": list(FRETISH_SPECS),
        "seeds": list(range(90)),
        # Measured worst case at this operating point is ~41s, so these are
        # 15-20x margin. A cap that bites records implies_ideal = 0 for a run
        # that was merely slow, which corrupts the response variable — strictly
        # worse than paying for the slow run.
        "timeout_caps": {"takeoff": 600, "fsm": 600, "fsm-timing": 600,
                         "fsm-combined": 900},
        "baseline_aliases": CJ_LARGE_ALIASES,
        "configs_dir": EXPERIMENTS_DIR / "configs-cj-large",
        "results_dir": EXPERIMENTS_DIR / "results-cj-large",
        "results_csv": EXPERIMENTS_DIR / "results-cj-large.csv",
        "default_jobs": 4,
    },
    # Direct-vs-log similarity metric as the sole crossed factor, at the same
    # large operating point as cj-large (generations=40, population_size=1000)
    # where repairs are strong enough for the metric to move outcomes. Only the
    # all-defaults C/default level runs — the metric is the experiment, so the
    # seed count is spent on statistical power for the main effect rather than
    # on a grid. No aliasing: a single level has nothing to alias onto.
    "metric": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": ["direct", "log"],
        "repair_modes": None,
        "sweeps": ["C"],
        "levels": {"C": ["default"]},
        "specs": list(FRETISH_SPECS),
        "seeds": list(range(100)),
        "timeout_caps": {"takeoff": 600, "fsm": 600, "fsm-timing": 600,
                         "fsm-combined": 900},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-metric",
        "results_dir": EXPERIMENTS_DIR / "results-metric",
        "results_csv": EXPERIMENTS_DIR / "results-metric.csv",
        "default_jobs": 4,
    },
    # Monolithic vs MUC-guided repair (tlsf.repair_mode) crossed with the TLSF
    # assumption/guarantee mutation split (sweep M), on the TLSF spec corpus —
    # repair_mode is a no-op on the FRETISH specs, so crossing it over them would
    # compare two identical arms. The split interacts with repair_mode: muc keeps
    # the environment side at full size while shrinking the guarantee side to the
    # minimal core, so at a fixed p_assumption it spends a larger share of its
    # guarantee mutations on the culprit formulae (verified: the assumption side
    # stays live in muc, so the factor is not confounded). Responses are
    # implies_ideal and wall_time_s co-equal — muc's pitch is quality where
    # monolithic sits at implies_ideal~=0 (gyro/lift/humanoid) AND runtime on the
    # heavy specs. Operating point gen10/pop200 (the config defaults), matching
    # the prior monolithic TLSF data for comparability. Generate the configs with
    #   python scripts/gen_configs.py --tlsf --sweeps M --repair both \
    #       --out-dir experiments/configs-muc
    # (--tlsf carries the 500 ms ltlsynt timeout and 0.15 scoring tolerance the
    # heavy specs need; --sweeps C would not — TLSF_SWEEPS has no C level.)
    "muc": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": ["mono", "muc"],
        "sweeps": ["M"],
        "levels": {"M": ["pg0.3", "pg0.5", "pg0.7", "pg0.9"]},
        "specs": list(TLSF_CORE_SPECS),
        # Seed-major disjoint ranges across av2/av3 (pass --seeds on launch); the
        # list is the ceiling a 60 h/machine deadline kill truncates to a
        # balanced design.
        "seeds": list(range(60)),
        # Caps sized to the slow (monolithic) arm so it is never censored — a cap
        # that bites records implies_ideal=0 and truncates wall_time_s for a run
        # that merely ran long, corrupting both responses. From the measured
        # gen10/pop200 monolithic means: humanoid 768 s (p90 1739 s), lift 73 s,
        # gyro 16 s, the rest single-digit. Applied identically to both arms so
        # muc's speed shows as a genuinely lower wall_time rather than a cap.
        "timeout_caps": {"humanoid-531": 2400, "lift": 600, "gyro-var1": 300,
                         "arbiter": 120, "lily02": 120, "minepump": 120},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-muc",
        "results_dir": EXPERIMENTS_DIR / "results-muc",
        "results_csv": EXPERIMENTS_DIR / "results-muc.csv",
        # jobs=1: one counter process per machine using its full 32-worker pool.
        # ltlsynt is multi-GB resident per call and max_concurrent_realizability
        # is per-process, so jobs>1 would multiply peak RAM and risk OOM; a single
        # process keeps the (uncapped, 128 GB) limit machine-wide.
        "default_jobs": 1,
    },
    # Follow-up to the muc campaign: sweep p_add_assumption (sweep P: 0.05, 0.15,
    # 0.3, 0.5) crossed with repair_mode {mono, muc}. The muc campaign found that
    # reaching an assumption-side ideal (e.g. arbiter's G F r0 & G F r1) is
    # bottlenecked on the fixed 0.05-rate p_add_assumption structural mutation, not
    # on the assumption/guarantee split; this tests whether raising it shifts
    # repairs from guarantee-weakening to the ideal. humanoid-531 is dropped: it
    # never reaches the ideal in either arm (so p_add_assumption cannot inform it)
    # and it is ~88% of the per-seed cost, so excluding it turns a ~12-seed
    # overnight run into a ~45-seed one on the specs where the lever operates.
    # Generate with:
    #   python scripts/gen_configs.py --tlsf --sweeps P --repair both \
    #       --out-dir experiments/configs-padd
    # (--tlsf carries the 60 s ltl2tgba-timeout, 500 ms ltlsynt-timeout and 0.15
    # scoring tolerance; the ltl2tgba-timeout needs the deployed counting-leak fix.)
    "padd": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": ["mono", "muc"],
        "sweeps": ["P"],
        "levels": {"P": ["padd0.05", "padd0.15", "padd0.3", "padd0.5"]},
        "specs": [s for s in TLSF_CORE_SPECS if s != "humanoid-531"],
        # Generous ceiling; a 12 h/machine overnight deadline truncates the
        # seed-major order to a balanced design at whatever depth it reaches.
        "seeds": list(range(160)),
        "timeout_caps": {"arbiter": 120, "gyro-var1": 300, "lift": 600,
                         "lily02": 120, "minepump": 120},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-padd",
        "results_dir": EXPERIMENTS_DIR / "results-padd",
        "results_csv": EXPERIMENTS_DIR / "results-padd.csv",
        "default_jobs": 1,
    },
    # Well-separation / output-assumption 2x2 (PR #34). The four arms are carried
    # as the levels of TLSF sweep W (gen_configs.TLSF_SWEEP_W), exactly as sweep
    # J carries the weakening ablation — so the arm lands in the level_name CSV
    # column and needs no crossed-factor plumbing. wsoff-oaoff is the
    # current-default control; wson-oaon the proposed configuration (output
    # assumptions admitted, the filter pruning the not-well-separated ones);
    # wsoff-oaon isolates output assumptions with no filter (the vacuous-repair
    # regression the filter is meant to prevent); wson-oaoff is a negative control
    # where the filter is inert without output-referencing assumptions to catch.
    # Runs the TLSF corpus, where an assumption can reference an output the system
    # controls so the filter actually fires (23.8% drop measured on arbiter),
    # unlike the FRETISH corpus where input-only assumptions leave it inert.
    # humanoid-531 is dropped as in padd (~88% of per-seed cost, no headroom).
    # Operating point gen10/pop200 (the TLSF config default). Generate with:
    #   python scripts/gen_configs.py --tlsf --sweeps W \
    #       --out-dir experiments/configs-wellsep
    "wellsep": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["W"],
        "levels": {"W": ["wsoff-oaoff", "wsoff-oaon",
                         "wson-oaoff", "wson-oaon"]},
        "specs": [s for s in TLSF_CORE_SPECS if s != "humanoid-531"],
        # Ceiling, not a target: seed-major so a wall-clock kill leaves a
        # balanced design. Calibration (2026-07-22) measured ~204 s per seed
        # (4 arms x 5 specs, jobs=1), so ~160 seeds/machine fits a 12 h budget
        # with margin and 320 total is a strong paired design; the original
        # 160-total design finishes in ~6 h. Split 0-159 / 160-319 across
        # av2/av3 (pass --seeds on launch).
        "seeds": list(range(320)),
        # Sized from a calibration on av2/av3 (2026-07-22): the heaviest arm
        # (wson-oaon) at gen10/pop200 maxed at arbiter 3s, gyro-var1 16s, lift
        # 27s, lily02 4s, minepump 8s over three seeds — the well-separation
        # query rarely fires (few candidates carry an output-referencing
        # assumption at p_add_assumption=0.05), so the overhead over baseline is
        # 0-30%. Caps are ~6-20x that max: generous enough never to censor a
        # slow-but-progressing run, tight enough to kill a true runaway in
        # minutes. Those caps were sized while ltlsynt_timeout_ms was 500, so
        # each query was bounded at half a second; it is 10000 from 2026-08-11
        # (see TLSF_LTLSYNT_TIMEOUT_MS in gen_configs.py), which raises the
        # per-query bound 20x. This profile's spec set is light enough that the
        # caps still hold, but any profile covering amba is not — recalibrate
        # before reusing a cap table across that change.
        # lift has a heavy runtime tail the 3-seed calibration missed (hard
        # seeds run 100-600s+ across all arms, intrinsic to the spec, not the
        # well-sep/output-assumption feature), so it gets the muc/padd 600s cap
        # rather than the calibrated 180s; the rest keep their calibrated caps.
        "timeout_caps": {"arbiter": 60, "gyro-var1": 120, "lift": 600,
                         "lily02": 60, "minepump": 60},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-wellsep",
        "results_dir": EXPERIMENTS_DIR / "results-wellsep",
        "results_csv": EXPERIMENTS_DIR / "results-wellsep.csv",
        "default_jobs": 1,
    },
    # Focused arbiter follow-up to the 2026-07-23 wellsep campaign. That run
    # showed the well-separation filter rejects *every* output-assumption repair
    # arbiter finds at gen10/pop200 (320/320 dropped) — all vacuous — but the
    # genuine well-separated repair (G(r -> F g), which the filter would KEEP)
    # was never reached at that budget. This asks whether a much larger operating
    # point (generations=40, population_size=2000; ~40x the work) lets the GA
    # find a well-separated output-assumption repair the filter keeps, i.e.
    # arbiter's wson-oaon found_repair rising above 0. Only two arms: wson-oaon
    # (treatment) and wson-oaoff (control — guarantee-only, isolates whether the
    # gain is the output-assumption path rather than higher-budget guarantee
    # weakening). arbiter only. Generate the pop2000/gen40 grid with:
    #   python scripts/gen_configs.py --tlsf --sweeps W \
    #       --generations 40 --population-size 2000 \
    #       --out-dir experiments/configs-arbiter-hp
    "arbiter-hp": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["W"],
        "levels": {"W": ["wson-oaoff", "wson-oaon"]},
        "specs": ["arbiter"],
        # Ceiling, not a target: seed-major so a 20 h/box kill leaves a balanced
        # two-arm design at whatever seed depth it reached. Operating point is
        # pop10000/gen100 (configs generated with --generations 100
        # --population-size 10000); calibration on av2 (2026-07-23) measured
        # ~171 s/run control, ~190 s treatment (~360 s/seed), so 20 h/box reaches
        # ~200 seeds. Split 0-199 / 200-399 across av2/av3 (pass --seeds).
        "seeds": list(range(400)),
        # Calibrated to ~600 s ≈ 3x the ~190 s/run wall time: arbiter has no heavy
        # runtime tail (unlike lift), so a run exceeding 600 s is a true ltlsynt
        # hang, not a slow-but-progressing seed. ltlsynt_timeout_ms=500 bounds
        # each per-candidate well-separation query.
        "timeout_caps": {"arbiter": 600},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-arbiter-hp",
        "results_dir": EXPERIMENTS_DIR / "results-arbiter-hp",
        "results_csv": EXPERIMENTS_DIR / "results-arbiter-hp.csv",
        "default_jobs": 1,
    },
    # Redistribution of the arbiter-hp budget after that run finished 0/~350 at
    # the fixed p_add_assumption=0.05. arbiter's only fix is the assumption pair
    # G F r0 & G F r1, and a single assumption yields no realizability payoff to
    # select on, so 0.05 leaves the pair unreachable. Sweep Q spreads seeds over
    # p_add_assumption in {0.1, 0.2, 0.4, 0.6, 0.8}, crossed with the two wson
    # arms (10 levels), at the same pop10000/gen100 operating point. The ~800-run
    # arbiter-hp budget is conserved: 80 seeds x 10 levels = 800 runs, split
    # 0-39 / 40-79 across av2/av3 (~400 runs/box, matching arbiter-hp's per-box
    # load), so each (padd, arm) cell gets 80 seed replications. Generate with:
    #   python scripts/gen_configs.py --tlsf --sweeps Q \
    #       --generations 100 --population-size 10000 \
    #       --out-dir experiments/configs-arbiter-padd
    "arbiter-padd": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["Q"],
        "levels": {},  # every generated Q level (all 10 padd x arm combos)
        "specs": ["arbiter"],
        "seeds": list(range(80)),
        "timeout_caps": {"arbiter": 600},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-arbiter-padd",
        "results_dir": EXPERIMENTS_DIR / "results-arbiter-padd",
        "results_csv": EXPERIMENTS_DIR / "results-arbiter-padd.csv",
        "default_jobs": 1,
    },
    # The basic-TLSF examples swept over generations (A) and population (B), the
    # two operating-point parameters, NSGA-II only, default weakening. These
    # specs are much slower than the FRETISH ones — three of them (lift,
    # gyro-var1, humanoid-531) take ~3 min even at the gen10/pop200 baseline and
    # scale with generations*population — so the grid is a coarse cross (four
    # levels per axis, sharing the baseline) rather than the fine FRETISH grid,
    # trading gradations for seeds within a fixed wall-clock budget. Reads its
    # own experiments/configs-tlsf/ grid; generate it with
    #   python scripts/gen_configs.py --tlsf
    "tlsf": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["A", "B"],
        "levels": {},
        "specs": list(TLSF_CORE_SPECS),
        # A generous ceiling, not a commitment. Ordering is seed-major, so a
        # machine killed at the wall-clock deadline leaves a balanced design at
        # whatever seed depth it reached; splitting 0-29/30-59 across av2/av3
        # keeps the two seed ranges disjoint so a truncated pair still merges to
        # one balanced dataset. ~16 h/machine is the real stop condition.
        "seeds": list(range(60)),
        # Flat per-spec caps generous enough that the largest operating point
        # (gen40 / pop500) never censors a slow-but-progressing run — a cap that
        # bites records implies_ideal = 0 for a run that merely ran long, which
        # corrupts the response variable. Sized from the measured baseline
        # (~190s for the heavy specs at gen10/pop200) times the ~4x operating
        # range times a safety margin for the jobs>1 thread-pool cap.
        "timeout_caps": {"arbiter": 2400, "gyro-var1": 3600,
                         "humanoid-531": 3600, "lift": 3600,
                         "lily02": 1200, "minepump": 1200},
        "baseline_aliases": TLSF_ALIASES,
        "configs_dir": EXPERIMENTS_DIR / "configs-tlsf",
        "results_dir": EXPERIMENTS_DIR / "results-tlsf",
        "results_csv": EXPERIMENTS_DIR / "results-tlsf.csv",
        # jobs=1: one counter process per machine, using its full internal
        # thread pool. The configs cap concurrent ltlsynt
        # (max_concurrent_realizability) to bound peak RAM, and that cap is
        # per-process, so a single process keeps it the machine-wide limit —
        # running several counter processes at once (jobs>1) would multiply the
        # ltlsynt count by jobs and risk the OOM the cap exists to prevent.
        "default_jobs": 1,
    },
    # FRETISH arm of the ablation campaign (PLAN §1): a full 2x2x2 factorial of
    # selection scheme (nsga2/weighted) x similarity metric (log/direct) x
    # Halstead weight (C/default's 0.1 vs C/no-halstead's 0.0) — 8 cells, so
    # interactions (does Halstead only matter under weighted?) are measurable,
    # not just main effects. Operating point generations=40/population_size=1000,
    # the metric-campaign scale where repairs are strong enough for the factors
    # to move outcomes. Control cell: nsga2 / log / C-default. Generate with
    #   python scripts/gen_configs.py --sweeps C --levels default,no-halstead \
    #       --schemes nsga2-truncate weighted --metric both \
    #       --generations 40 --population-size 1000 \
    #       --out-dir experiments/configs-ablate-fret
    # (--levels keeps sweep C's other three levels out of the grid — they would
    # silently inflate the 8 cells to 20.)
    "ablate-fret": {
        "schemes": ["nsga2-truncate", "weighted"],
        "weakenings": None,
        "metrics": ["direct", "log"],
        "repair_modes": None,
        "sweeps": ["C"],
        "levels": {"C": ["default", "no-halstead"]},
        "specs": list(FRETISH_SPECS),
        "seeds": list(range(30)),
        # The cj-large/metric caps for the same operating point: measured worst
        # case there was ~41s, so these are 15-20x margin — a cap that bites
        # records implies_ideal = 0 for a run that was merely slow.
        "timeout_caps": {"takeoff": 600, "fsm": 600, "fsm-timing": 600,
                         "fsm-combined": 900},
        # Only sweep C runs, so there is no A/gen baseline to alias onto.
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-ablate-fret",
        "results_dir": EXPERIMENTS_DIR / "results-ablate-fret",
        "results_csv": EXPERIMENTS_DIR / "results-ablate-fret.csv",
        "default_jobs": 4,
    },
    # TLSF arm of the same 8-cell factorial, over the 13 fixes-backed TLSF
    # families at the TLSF operating point (gen10/pop200, the config defaults).
    # A flat 600 s per-run cap (the arbiter-hp precedent) bounds the heavy tail
    # — humanoid means 768 s uncapped with a 1739 s p90, and the corpus mean of
    # ~150 s/run is what makes 8 x 13 x 15 fit the campaign window; the
    # censoring it introduces is deliberate and applied identically to every
    # cell. The configs also need the tlsf-profile runtime settings (500 ms
    # ltlsynt timeout, 60 s ltl2tgba timeout, 0.15 scoring-failure tolerance),
    # passed explicitly because --tlsf swaps in the TLSF sweep table, which has
    # no sweep C. Generate with
    #   python scripts/gen_configs.py --sweeps C --levels default,no-halstead \
    #       --schemes nsga2-truncate weighted --metric both \
    #       --ltlsynt-timeout 500 --ltl2tgba-timeout 60000 \
    #       --max-scoring-failure-rate 0.15 \
    #       --out-dir experiments/configs-ablate-tlsf
    # The status objective's grading scale (sweep G: tiered vs mrs), over the
    # TLSF ablation corpus at the config defaults. Everything but the grading is
    # held fixed -- one scheme, one metric, one operating point -- because the
    # question is whether the finer scale changes what the search finds, and
    # crossing more factors would spend the window on power the primary
    # comparison needs. See experiments/2026-08-11-status-grading/PLAN.md, which
    # pre-registers the endpoints and deliberately registers no decision rule.
    #
    # Generate with
    #   python scripts/gen_configs.py --tlsf --sweeps G \
    #       --ltlsynt-timeout 10000 \
    #       --out-dir experiments/configs-status-grading
    #
    # Per-spec caps at 4x the slowest run a 3-seed pilot over this exact corpus
    # and operating point measured, floored at 300s and rounded to the minute.
    # 4x rather than the 6-20x the older tables used: those were sized when
    # nothing bounded an individual ltlsynt query, whereas ltlsynt_timeout_ms is
    # 10000 here, so the per-run tail is far shorter. amba earns 4320s against
    # the flat 600s of ablate-tlsf -- its mrs arm alone measured 1078s -- and
    # capping it at 600s would censor the arm rather than measure it.
    "status-grading": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["G"],
        "levels": {"G": ["tiered", "mrs"]},
        "specs": list(TLSF_ABLATION_SPECS),
        "seeds": list(range(24)),
        "timeout_caps": {
            "amba": 4320, "humanoid-531": 1560, "lift": 840,
            "simple-arbiter": 720, "round-robin-arbiter": 660,
            "humanoid-458": 540, "full-arbiter": 420, "gyro-var1": 420,
            "gyro-var2": 420, "detector": 360, "arbiter": 300,
            "arbiter-handshake": 300, "codesample-un1": 300,
            "codesample-un2": 300, "lily02": 300, "load-balancer": 300,
            "minepump": 300, "prioritized-arbiter": 300, "rg1": 300,
            "rg2": 300,
        },
        # Sweep G has no gen/pop axis, so there is no A/gen baseline to alias.
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-status-grading",
        "results_dir": EXPERIMENTS_DIR / "results-status-grading",
        "results_csv": EXPERIMENTS_DIR / "results-status-grading.csv",
        # jobs=1 for the same RAM reason as the other TLSF profiles: ltlsynt is
        # multi-GB per call on these specs and the cap is per counter process.
        "default_jobs": 1,
    },
    "ablate-tlsf": {
        "schemes": ["nsga2-truncate", "weighted"],
        "weakenings": None,
        "metrics": ["direct", "log"],
        "repair_modes": None,
        "sweeps": ["C"],
        "levels": {"C": ["default", "no-halstead"]},
        "specs": list(TLSF_ABLATION_SPECS),
        "seeds": list(range(15)),
        # A flat 600s over the whole set, sized while ltlsynt_timeout_ms was
        # 500. amba no longer fits it: at the 10000 budget a gen5/pop100 probe
        # took 404s, so gen10/pop200 will run past this cap and be censored —
        # trading its old zero-yield for a timeout rather than a result.
        # Recalibrate amba (and re-check the rest) before launching; see
        # TLSF_LTLSYNT_TIMEOUT_MS in gen_configs.py.
        "timeout_caps": {s: 600 for s in TLSF_ABLATION_SPECS},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-ablate-tlsf",
        "results_dir": EXPERIMENTS_DIR / "results-ablate-tlsf",
        "results_csv": EXPERIMENTS_DIR / "results-ablate-tlsf.csv",
        # jobs=1 for the same RAM reason as the tlsf profile: ltlsynt is
        # multi-GB resident and its concurrency cap is per counter process.
        "default_jobs": 1,
    },
    # Head-to-head top-up against the AuRUS baseline (scripts/aurus_campaign.py):
    # the control cell only (nsga2 / log metric / C-default weights), extended
    # to seeds 0-19 and to the H2H_TLSF_SPECS corpus, so every family AuRUS
    # runs has 20 counter control runs to compare against. Dedup with
    # ablate-tlsf is by construction: this profile shares ablate-tlsf's
    # configs dir, results dir, and results CSV, and the resume key (sweep,
    # level, selection, weakening, metric, repair_mode, spec, seed) fully
    # identifies a control-cell row — so the (spec, seed) rows ablate-tlsf
    # already completed are skipped and only the top-up executes:
    # arbiter-aurus at seeds 0-19 plus the 11 shared families at seeds 15-19.
    # The same sharing means the two profiles must not run concurrently on
    # one machine (interleaved CSV appends and run dirs).
    "h2h-tlsf": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        # ["log"] (not None): keeps the factor-cell key, run_id and CSV metric
        # column identical to ablate-tlsf's log cell, which is what makes the
        # shared-CSV dedup hold.
        "metrics": ["log"],
        "repair_modes": None,
        "sweeps": ["C"],
        "levels": {"C": ["default"]},
        "specs": H2H_TLSF_SPECS,
        "seeds": list(range(20)),
        "timeout_caps": {s: 600 for s in H2H_TLSF_SPECS},
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-ablate-tlsf",
        "results_dir": EXPERIMENTS_DIR / "results-ablate-tlsf",
        "results_csv": EXPERIMENTS_DIR / "results-ablate-tlsf.csv",
        "default_jobs": 1,
    },
    # nsga2 vs nsga2-replicate on FRETISH, at the gen40/pop1000 operating point
    # the cj-large and metric campaigns used — so the control arm is checkable
    # against their rows rather than being taken on trust. Three arms:
    #   nsga2-truncate/sweep_R    control
    #   nsga2-apportion/sweep_R   treatment
    #   nsga2-truncate/sweep_S    compute-matched control (see gen_configs)
    # The design is the config tree, not this table: sweep S is generated under
    # the truncate scheme only, so listing both schemes and both sweeps still
    # yields three arms rather than four. Generate with
    #   python scripts/gen_configs.py --schemes nsga2-truncate nsga2-apportion \
    #       --sweeps R --generations 40 --population-size 1000 \
    #       --out-dir experiments/configs-replicate
    #   python scripts/gen_configs.py --schemes nsga2-truncate --sweeps S \
    #       --generations 40 --population-size 1000 \
    #       --compute-match-factor <measured> \
    #       --out-dir experiments/configs-replicate
    #
    # Responses: implies_ideal (quality) with fsm carrying the power — it is the
    # only FRETISH family measured off both bounds (0.46-0.53 across cj-large
    # and metric) — fsm-combined (0.08) testing upward movement, and
    # takeoff/fsm-timing (~1.0) as saturation controls that should not move.
    # found_repair, n_repairs and wall_time_s are secondary.
    "replicate": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R", "S"],
        "levels": {},
        "specs": list(FRETISH_SPECS),
        # Seed-major disjoint ranges across av2/av3 (pass --seeds on launch).
        # 200 pairs per cell resolves ~0.15 absolute on implies_ideal at fsm's
        # p~=0.5, ~0.10 pooled over the two elitism levels. Smaller effects are
        # out of reach at any budget this campaign can afford — 0.05 would need
        # ~1600 seeds per arm.
        "seeds": list(range(200)),
        # cj-large's caps for this operating point, scaled for the two arms that
        # cost more: replicate breeds from a replicated population and sweep S
        # runs a longer generation budget. Sized never to bite — a cap that
        # fires records implies_ideal = 0 for a run that was merely slow.
        "timeout_caps": {"takeoff": 900, "fsm": 900, "fsm-timing": 900,
                         "fsm-combined": 1500},
        # The treatment arm's whole claim is that it keeps more distinct
        # candidates, which shows up as more repairs, and compare's cost scales
        # with them. At the default 600 s the arm under test would be the one
        # whose comparisons get censored.
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-replicate",
        "results_dir": EXPERIMENTS_DIR / "results-replicate",
        "results_csv": EXPERIMENTS_DIR / "results-replicate.csv",
        "default_jobs": 4,
    },
    # The weakening cross of the FRETISH comparison, on the two specs with
    # headroom. `replicate` runs at the shipped run_weakening = true, where the
    # filter drops about 70% of each generation's offspring -- so it measures
    # replicate's advantage in a configuration that discards most of what
    # breeding produces, and the 2026-07-15 cj-large campaign found that turning
    # the filter off raises implies_ideal. This arm runs the same sweep R cells
    # with the filter off, to say whether "replicate keeps more distinct
    # candidates" survives the stage that throws most of them away.
    #
    # Only fsm and fsm-combined: takeoff and fsm-timing sit at implies_ideal
    # ~1.0, so a second factor buys nothing there. Arm C is not crossed in --
    # compute-matching is a statement about the scheme's cost, inherited from
    # the wkon arm -- so this is a two-arm comparison.
    #
    # It shares `replicate`'s results directory and CSV deliberately: the
    # weakening column separates the rows, run_id carries a _wkoff tag so no run
    # directory collides with the flat-layout runs, and the analysis reads one
    # file. Generate the configs it reads with
    #   python scripts/gen_configs.py --schemes nsga2-truncate nsga2-apportion \
    #       --sweeps R --weakening off --generations 40 --population-size 1000 \
    #       --out-dir experiments/configs-replicate
    #
    # The caps are 3600 s, not the wkon arm's 900/1500. Sweep R's partial results
    # showed the tighter caps censoring only the replicate arm -- 250 of 352
    # fsm-timing pairs, 56 of 358 fsm-combined -- because they were sized off
    # nsga2's costs and replicate runs several times longer. A cap that fires on
    # one arm and never on the other is a one-sided filter on the response under
    # test, so this arm gets a cap loose enough to bind on neither.
    "replicate-wkoff": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": ["wkoff"],
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R"],
        "levels": {},
        "specs": ["fsm", "fsm-combined"],
        "seeds": list(range(200)),
        "timeout_caps": {"fsm": 3600, "fsm-combined": 3600},
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-replicate",
        "results_dir": EXPERIMENTS_DIR / "results-replicate",
        "results_csv": EXPERIMENTS_DIR / "results-replicate.csv",
        "default_jobs": 4,
    },
    # Sweep R again at a cap that binds on neither arm, to replace the rows the
    # original caps censored. Sweep R sized its timeouts off nsga2's costs, and
    # replicate runs several times longer, so the cap fired on the treatment arm
    # alone -- 250 of 352 fsm-timing pairs, 56 of 358 fsm-combined, 34 of 354
    # takeoff, against zero for nsga2 in every cell. A one-sided cap on the
    # response under test is a bias, not a budget. fsm is absent because nothing
    # censored there.
    #
    # It shares `replicate`'s CSV and results dir so resume does the selection:
    # delete the censored rows (and their run directories), point this profile at
    # the same cells, and only the deleted ones execute. Recap rows are therefore
    # indistinguishable from originals by key -- deliberately, since the analysis
    # wants one row per cell -- but recoverable from the data, as any surviving
    # row censored at the old cap still reads timed_out=1 at wall_time_s == 900
    # or 1500.
    #
    # fsm-timing is expected to be run on a seed subsample via --seeds: it scores
    # implies_ideal 1.000 in both arms on every pair that completed, so its
    # re-run buys an honest wall-time ratio rather than a quality contrast, and
    # the full 250 costs about 31 h per machine against roughly 6 h for 50 seeds.
    "replicate-recap": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R"],
        "levels": {},
        "specs": ["takeoff", "fsm-timing", "fsm-combined"],
        "seeds": list(range(200)),
        "timeout_caps": {
            "takeoff": 3600, "fsm-timing": 3600, "fsm-combined": 3600},
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-replicate",
        "results_dir": EXPERIMENTS_DIR / "results-replicate",
        "results_csv": EXPERIMENTS_DIR / "results-replicate.csv",
        "default_jobs": 4,
    },
    # The TLSF half of the same comparison, at the TLSF baseline operating point
    # (gen10/pop200) for comparability with the prior TLSF data. Two arms only:
    # the compute-matched control rides on the FRETISH half, where the responses
    # are less saturated and a run costs minutes rather than tens of minutes.
    # Generate with
    #   python scripts/gen_configs.py --tlsf \
    #       --schemes nsga2-truncate nsga2-apportion \
    #       --sweeps R --out-dir experiments/configs-replicate-tlsf
    # (--tlsf carries the 500 ms ltlsynt timeout, 60 s ltl2tgba timeout and 0.15
    # scoring tolerance the heavy specs need.)
    #
    # Specs chosen by measured headroom from the 2026-07-21 muc campaign at this
    # operating point: arbiter (found_repair 0.50) and lily02 (implies_ideal
    # 0.67) are the two that can move; gyro-var1, lift and minepump sit at
    # implies_ideal 0.000 with found_repair ~1.0 and serve as negative controls.
    # humanoid-531 is excluded on cost alone (965 s mean, 2400 s p90).
    "replicate-tlsf": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R"],
        "levels": {},
        "specs": ["arbiter", "gyro-var1", "lift", "lily02", "minepump"],
        "seeds": list(range(60)),
        # The muc profile's caps, roughly doubled: the treatment arm runs longer
        # and its extra survivors each cost realizability checks.
        "timeout_caps": {"arbiter": 300, "gyro-var1": 600, "lift": 1200,
                         "lily02": 300, "minepump": 300},
        "compare_timeout": 1200,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-replicate-tlsf",
        "results_dir": EXPERIMENTS_DIR / "results-replicate-tlsf",
        "results_csv": EXPERIMENTS_DIR / "results-replicate-tlsf.csv",
        # jobs=1 for the same RAM reason as the tlsf profile: ltlsynt is
        # multi-GB resident per call on these specs.
        "default_jobs": 1,
    },
    # Does nsga2-apportion's diversity unlock the arbiter family, or only the one
    # `arbiter` spec? See experiments/2026-08-10-arbiter-probe/PLAN.md for the
    # pre-registered decision rule. This is a generality probe, not a rerun of
    # 2026-07-31-replicate: that campaign's decision failed on FRETISH cost and
    # on the compute-matched control, neither of which this can reopen, and it
    # deliberately carries no arm C for the same reason.
    #
    # Both schemes, one level. Sweep R restricted to elit0.1 pins elitism_rate
    # into the archived config at the shipped default rather than inheriting it,
    # because 2026-08-07-elitism may yet move that default and an inherited key
    # would silently restate what this campaign ran under.
    #
    # Generate the configs with
    #   python scripts/gen_configs.py --tlsf \
    #       --schemes nsga2-truncate nsga2-apportion --sweeps R --levels elit0.1 \
    #       --out-dir experiments/configs-arbiter-probe
    "arbiter-probe": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R"],
        "levels": {"R": ["elit0.1"]},
        "specs": list(ARBITER_PROBE_SPECS),
        # 40 seeds x 9 arbiter families = 360 pooled pairs. The effect under test
        # is the replicate arbiter split (0/120 vs 120/120); 40 pairs per family
        # resolve ~0.3 absolute on found_repair, which is far coarser than that
        # split and still fine enough to separate "unlocks the family" from
        # "unlocked one spec". Seed-major disjoint ranges across av2/av3.
        "seeds": list(range(40)),
        # elitism-tlsf's flat 900 s on the same operating point and hosts. Sized
        # never to bite: replicate censored its treatment arm alone with caps cut
        # to nsga2's costs, and apportion is the treatment arm here too.
        "timeout_caps": {spec: 900 for spec in ARBITER_PROBE_SPECS},
        # apportion's whole claim is more distinct survivors, so it returns more
        # repairs and compare's cost scales with them — the same asymmetry that
        # censored replicate's comparisons at the 600 s default.
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-arbiter-probe",
        "results_dir": EXPERIMENTS_DIR / "results-arbiter-probe",
        "results_csv": EXPERIMENTS_DIR / "results-arbiter-probe.csv",
        # jobs=1 for the same RAM reason as every other TLSF profile.
        "default_jobs": 1,
    },
    # ── 2026-08-11-selection-default ────────────────────────────────────────
    # Whether nsga2-apportion should replace nsga2-truncate as the shipped
    # default. See experiments/2026-08-11-selection-default/PLAN.md for the
    # pre-registered decision rule, its five criteria and the 0.05
    # non-inferiority margin with the power calculation behind it.
    #
    # Three arms, and they are split across two profiles per path rather than
    # crossed inside one. A single profile listing both schemes and both sweeps
    # emits the full product, which would add a fourth cell -- apportion at
    # matched generations -- that no criterion reads and that costs a third of
    # the campaign's wall time. The -cm profiles therefore carry the compute
    # control alone, and share the CSV and run directory of their R half so the
    # three arms merge and analyse as one dataset. KEY_FIELDS includes `sweep`
    # and `level_name`, so R/elit0.1 and S/cm-elit0.1 are distinct keys and the
    # sharing cannot collide.
    #
    # The corpus is deliberately not enriched: every FRETISH spec and the whole
    # 20-family TLSF corpus, with `arbiter` one family of twenty. 2026-08-10
    # measured its effect on a corpus picked because that effect was there,
    # which is why it could not speak to a default.
    #
    # Generate the R halves with
    #   python scripts/gen_configs.py [--tlsf] \
    #       --schemes nsga2-truncate nsga2-apportion --sweeps R --levels elit0.1 \
    #       --out-dir experiments/configs-seldefault[-tlsf]
    # and the compute control with the ratio measured in calibration (PLAN §5):
    #   python scripts/gen_configs.py [--tlsf] --schemes nsga2-truncate \
    #       --sweeps S --levels cm-elit0.1 --compute-match-factor <measured> \
    #       --out-dir experiments/configs-seldefault[-tlsf]
    "seldefault-fret": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R"],
        "levels": {"R": ["elit0.1"]},
        "specs": list(FRETISH_SPECS),
        # 4 specs x 70 seeds = 280 pairs per contrast, which is what PLAN §7's
        # power table requires for the 0.05 margin to clear at a true adverse
        # effect of -0.01, given the 0.341 paired SD measured on the elitism
        # campaign's 600 FRETISH pairs.
        "seeds": list(range(70)),
        "timeout_caps": {"takeoff": 900, "fsm": 900, "fsm-timing": 900,
                         "fsm-combined": 1800},
        # As arbiter-probe: apportion returns more repairs, and compare's cost
        # scales with them.
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-seldefault",
        "results_dir": EXPERIMENTS_DIR / "results-seldefault",
        "results_csv": EXPERIMENTS_DIR / "results-seldefault.csv",
        # 4 as on every FRETISH profile; ltlsynt is not in play here, so the
        # per-call RAM ceiling that pins the TLSF profiles to 1 does not bind.
        # Both halves must agree: wall_time_s is a response variable and arm C
        # is defined by it, so a contention difference between the arms would
        # land straight in the ratio that sets arm C's own generation count.
        "default_jobs": 4,
    },
    "seldefault-fret-cm": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["S"],
        "levels": {"S": ["cm-elit0.1"]},
        "specs": list(FRETISH_SPECS),
        "seeds": list(range(70)),
        # Arm C runs more generations than arm A by construction, so it must not
        # inherit a cap sized for A: censoring the control alone is how replicate
        # lost its comparison.
        "timeout_caps": {"takeoff": 1800, "fsm": 1800, "fsm-timing": 1800,
                         "fsm-combined": 3600},
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-seldefault",
        "results_dir": EXPERIMENTS_DIR / "results-seldefault",
        "results_csv": EXPERIMENTS_DIR / "results-seldefault.csv",
        "default_jobs": 4,
    },
    "seldefault-tlsf": {
        "schemes": ["nsga2-truncate", "nsga2-apportion"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["R"],
        "levels": {"R": ["elit0.1"]},
        "specs": list(TLSF_ABLATION_SPECS),
        # 20 families x 25 seeds = 500 pairs, above the 420 PLAN §7 requires at
        # the 0.418 paired SD measured on the arbiter probe's 400 TLSF pairs.
        "seeds": list(range(25)),
        "timeout_caps": {spec: 900 for spec in TLSF_ABLATION_SPECS},
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-seldefault-tlsf",
        "results_dir": EXPERIMENTS_DIR / "results-seldefault-tlsf",
        "results_csv": EXPERIMENTS_DIR / "results-seldefault-tlsf.csv",
        # jobs=1 for the same RAM reason as every other TLSF profile.
        "default_jobs": 1,
    },
    "seldefault-tlsf-cm": {
        "schemes": ["nsga2-truncate"],
        "weakenings": None,
        "metrics": None,
        "repair_modes": None,
        "sweeps": ["S"],
        "levels": {"S": ["cm-elit0.1"]},
        "specs": list(TLSF_ABLATION_SPECS),
        "seeds": list(range(25)),
        # Raised over the R half for the same reason as seldefault-fret-cm.
        "timeout_caps": {spec: 1800 for spec in TLSF_ABLATION_SPECS},
        "compare_timeout": 1800,
        "baseline_aliases": {},
        "configs_dir": EXPERIMENTS_DIR / "configs-seldefault-tlsf",
        "results_dir": EXPERIMENTS_DIR / "results-seldefault-tlsf",
        "results_csv": EXPERIMENTS_DIR / "results-seldefault-tlsf.csv",
        "default_jobs": 1,
    },
}


def verify_aliases(configs_by_key: dict[tuple[str, str, str, str, str, str], Path],
                   scheme: str, weakening: str, metric: str, repair: str,
                   aliases: dict) -> dict:
    """Return the alias map restricted to pairs whose files are byte-identical."""
    active: dict[tuple[str, str], tuple[str, str]] = {}
    for alias, canon in aliases.items():
        a_path = configs_by_key.get((scheme, weakening, metric, repair, *alias))
        c_path = configs_by_key.get((scheme, weakening, metric, repair, *canon))
        if a_path is None:
            continue  # alias config not generated; nothing to alias
        if c_path is None or a_path.read_bytes() != c_path.read_bytes():
            print(f"WARN: {scheme}/{weakening}/{metric}/{repair}/{a_path.name} "
                  f"is not byte-identical to canonical "
                  f"sweep_{canon[0]}_{canon[1]}.toml — treating as distinct")
            continue
        active[alias] = canon
    return active


# ── Metadata ─────────────────────────────────────────────────────────────────

def level_value_of(level_name: str) -> int | float | str:
    """Trailing number if present ('gen10' → 10, 'ptrig0.75' → 0.75).

    The fractional form matters: matching only \\d+ would read 'ptrig0.75' as
    75 and sort it above 'ptrig0.5'.
    """
    m = re.search(r"(\d+\.\d+|\d+)$", level_name)
    if not m:
        return level_name
    return float(m.group(1)) if "." in m.group(1) else int(m.group(1))


def _factor_dir(config_path: Path, known: tuple[str, ...] | frozenset[str]):
    """First ancestor directory whose name is in `known`, or None.

    Scans ancestors rather than assuming a fixed depth, so a factor is found
    wherever it nests. With metric crossed in the layout is up to three deep
    (<scheme>/<weakening>/<metric>/sweep_*.toml), and a parent/parent.parent
    walk would mis-attribute the segments.
    """
    for p in config_path.parents:
        if p.name in known:
            return p.name
    return None


def scheme_of(config_path: Path) -> str:
    """Selection scheme: the first ancestor that is not a factor directory.

    Canonicalised, so a config tree generated under the old scheme names still
    reads back as the name the `selection` column now carries -- the same value
    load_done_set() derives from an archived CSV, which is what lets the two
    meet on the resume key.
    """
    for p in config_path.parents:
        if p.name not in FACTOR_DIRS:
            return canonical_scheme(p.name)
    return canonical_scheme(config_path.parent.name)


def weakening_of(config_path: Path) -> str:
    """Weakening state from the config's ancestor directories."""
    return _factor_dir(config_path, WEAKENING_DIRS) or LEGACY_WEAKENING


def metric_of(config_path: Path) -> str:
    """Similarity metric (short label) from the config's ancestor directories."""
    return _factor_dir(config_path, METRIC_DIRS) or LEGACY_METRIC


def repair_mode_of(config_path: Path) -> str:
    """Repair mode (short label) from the config's ancestor directories."""
    return _factor_dir(config_path, REPAIR_DIRS) or LEGACY_REPAIR


def extract_metadata(config_path: Path) -> tuple:
    """Return (sweep, level_name, level_value) from a config filename."""
    stem = config_path.stem  # e.g. 'sweep_A_gen10'
    parts = stem.split("_", 2)  # ['sweep', 'A', 'gen10']
    return parts[1], parts[2], level_value_of(parts[2])


def counter_timeout(level_name: str, level_value) -> int:
    """Return a generous per-run timeout in seconds (full profile)."""
    if isinstance(level_value, int) and level_name.startswith("gen"):
        return max(120, level_value * 90)
    if isinstance(level_value, int) and level_name.startswith("pop"):
        return max(120, level_value // 2)
    return 300


# ── Parsing ───────────────────────────────────────────────────────────────────

def parse_repair_files(output_dir: Path) -> tuple[int, float]:
    """Return (n_repairs, best_fitness) from a run's repair files.

    FRETISH runs write repair_N.json with the weighted total nested under a
    "fitness" object. TLSF runs write repair_N.tlsf beside a repair_N.fitness.json
    sidecar carrying the total at top level. The sidecar also matches
    repair_*.json, so TLSF is detected first (by its .tlsf files) to avoid
    counting sidecars as repairs and reading a missing nested "fitness".
    """
    tlsf_files = list(output_dir.glob("repair_*.tlsf"))
    if tlsf_files:
        best = float("-inf")
        for f in tlsf_files:
            try:
                total = json.loads(
                    f.with_suffix(".fitness.json").read_text()).get("total")
                if total is not None:
                    best = max(best, float(total))
            except Exception:
                pass
        return len(tlsf_files), best if best != float("-inf") else float("nan")

    files = list(output_dir.glob("repair_*.json"))
    if not files:
        return 0, float("nan")
    best = float("-inf")
    for f in files:
        try:
            data = json.loads(f.read_text())
            total = (data.get("fitness") or {}).get("total")
            if total is not None:
                best = max(best, float(total))
        except Exception:
            pass
    return len(files), best if best != float("-inf") else float("nan")


def parse_dropped(log_path: Path) -> int:
    """Individuals counter dropped after a fitness function threw (0 if none)."""
    try:
        text = log_path.read_text(errors="replace")
    except OSError:
        return 0
    m = DROPPED_RE.search(text)
    return int(m.group(1)) if m else 0


def tail_line(log_path: Path) -> str:
    """Return the last non-blank line of a log, for console error context.

    counter draws progress with carriage returns, so a raw line may pack many
    updates; keep only the final segment after the last '\\r'.
    """
    try:
        text = log_path.read_text(errors="replace")
    except OSError:
        return ""
    for raw in reversed(text.splitlines()):
        line = raw.split("\r")[-1].strip()
        if line:
            return line
    return ""


def parse_compare_output(stdout: str) -> tuple[str, int, int]:
    """Return (best_relation, implies_ideal, n_implies) from compare stdout.

    implies_ideal is 1 when at least one repair is equivalent or strictly
    stronger than an ideal (i.e. the repair is at least as strong).
    """
    m = SUMMARY_RE.search(stdout)
    if not m:
        return "unknown", 0, 0

    groups = list(map(int, m.groups()))
    n_equiv, n_stronger = groups[0], groups[1]
    n_implies = n_equiv + n_stronger

    best_priority = -1
    best_relation = "timeout"
    for line in stdout.splitlines():
        lm = PER_REPAIR_RE.match(line.strip())
        if lm:
            rel = lm.group(1)
            if RELATION_PRIORITY.get(rel, -1) > best_priority:
                best_priority = RELATION_PRIORITY[rel]
                best_relation = rel

    return best_relation, int(n_implies > 0), n_implies


# ── Binary provenance ────────────────────────────────────────────────────────

def parse_version_output(text: str) -> dict[str, str]:
    """Parse `<binary> --version` output into its key=value pairs.

    The binary prints one `key=value` line per fact (commit, commit_short,
    dirty). Lines without a `=` are ignored, so a future banner line cannot
    break the parse.
    """
    fields: dict[str, str] = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if sep:
            fields[key.strip()] = value.strip()
    return fields


def binary_version(bin_path: Path) -> dict[str, str]:
    """Ask a binary what it was built from. Once per campaign, not per run.

    Deliberately not `git rev-parse`: the working tree says what the source is
    now, and the binary says what it was compiled from. Those diverge every
    time a fix lands without a rebuild, and that gap is exactly what a recorded
    commit is for. A binary that predates --version exits non-zero and reads as
    LEGACY_COMMIT, which the staleness check then treats as stale — it is, by
    definition, older than this script.
    """
    unknown = {"commit": LEGACY_COMMIT, "commit_short": LEGACY_COMMIT,
               "dirty": ""}
    try:
        proc = subprocess.run([str(bin_path), "--version"], check=True,
                              timeout=60, capture_output=True, text=True)
    except (OSError, subprocess.SubprocessError):
        return unknown
    fields = parse_version_output(proc.stdout)
    if "commit" not in fields:
        return unknown
    return {**unknown, **fields}


def working_tree_head() -> str | None:
    """HEAD of the checkout this script lives in, or None outside a work tree."""
    try:
        proc = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "rev-parse", "HEAD"],
            check=True, timeout=60, capture_output=True, text=True)
    except (OSError, subprocess.SubprocessError):
        return None
    return proc.stdout.strip() or None


def git_describe() -> str:
    try:
        proc = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "describe", "--always", "--dirty",
             "--tags"],
            check=True, timeout=60, capture_output=True, text=True)
    except (OSError, subprocess.SubprocessError):
        return LEGACY_COMMIT
    return proc.stdout.strip() or LEGACY_COMMIT


def git_branch() -> str:
    try:
        proc = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "rev-parse", "--abbrev-ref", "HEAD"],
            check=True, timeout=60, capture_output=True, text=True)
    except (OSError, subprocess.SubprocessError):
        return LEGACY_COMMIT
    return proc.stdout.strip() or LEGACY_COMMIT


def staleness_problems(versions: dict[str, dict[str, str]],
                       head: str | None) -> list[str]:
    """Reasons the binaries must not be trusted to represent HEAD.

    Empty means every binary reports a clean build of the working tree's HEAD.
    """
    problems: list[str] = []
    for name, version in versions.items():
        if version["commit"] == LEGACY_COMMIT:
            problems.append(
                f"{name} does not report a commit: it predates --version, or "
                f"was built outside a git work tree")
            continue
        if version.get("dirty") == "1":
            problems.append(
                f"{name} was built from a modified working tree "
                f"({version['commit_short']}-dirty), so its commit does not "
                f"identify the code that ran")
        if head is not None and version["commit"] != head:
            problems.append(
                f"{name} was built from {version['commit_short']}, but the "
                f"working tree is at {head[:7]}")
    return problems


def enforce_binary_freshness(versions: dict[str, dict[str, str]],
                             head: str | None, allow_stale: bool) -> None:
    problems = staleness_problems(versions, head)
    if not problems:
        return
    bar = "!" * 72
    print(f"\n{bar}")
    print("STALE BINARY: the binaries do not match this working tree.")
    for problem in problems:
        print(f"  - {problem}")
    print("A campaign run against a binary that predates a fix produces "
          "numbers\nthat look valid and are not. Rebuild "
          "(cmake --workflow --preset release)\nor pass --allow-stale-binary "
          "if the mismatch is deliberate.")
    print(f"{bar}\n")
    if not allow_stale:
        sys.exit("Refusing to start with a stale binary.")
    print("Continuing anyway (--allow-stale-binary).\n")


def write_manifest(path: Path, profile_name: str, profile: dict,
                   versions: dict[str, dict[str, str]], head: str | None,
                   jobs: int, specs: list, seeds: list,
                   sweeps: set | None) -> None:
    """Record what this host ran, beside the CSV it appends to.

    The CSV carries the binary commit per row, but nothing else about the
    launch: which branch, which levels, how many workers. Reconstructing that
    afterwards from config mtimes is the guesswork the PROVENANCE.json files
    under experiments/ document; this writes it down at run time instead.
    """
    manifest = {
        "profile": profile_name,
        "hostname": socket.gethostname(),
        "started": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "git": {
            "branch": git_branch(),
            "describe": git_describe(),
            "head": head or LEGACY_COMMIT,
        },
        "binaries": {
            name: {"path": str(bin_path), **versions[name]}
            for name, bin_path in (("counter", COUNTER_BIN),
                                   ("compare", COMPARE_BIN))
        },
        "sweep": {
            "schemes": profile["schemes"],
            "weakenings": profile["weakenings"],
            "metrics": profile["metrics"],
            "repair_modes": profile["repair_modes"],
            "sweeps": sorted(sweeps) if sweeps is not None else None,
            # Restricted to the sweeps this launch asked for: a profile's level
            # map covers sweeps a --sweeps run never touches, and recording
            # those makes the manifest describe a campaign that did not happen.
            "levels": {k: v for k, v in profile["levels"].items()
                       if sweeps is None or k in sweeps},
            "specs": list(specs),
            "seeds": list(seeds),
            "jobs": jobs,
            "configs_dir": str(profile["configs_dir"]),
            "results_dir": str(profile["results_dir"]),
            "results_csv": str(profile["results_csv"]),
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(manifest, indent=2) + "\n")


def manifest_path(results_csv: Path) -> Path:
    """One manifest per host beside the CSV; campaigns run on av2 and av3 at
    once and merge into one file, so a shared name would have them overwrite
    each other."""
    host = socket.gethostname().split(".")[0]
    return results_csv.with_name(f"{results_csv.stem}-manifest-{host}.json")


# ── CSV helpers ───────────────────────────────────────────────────────────────

def load_done_set(csv_path: Path) -> set:
    done: set = set()
    if not csv_path.exists():
        return done
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            done.add((row["sweep"], row["level_name"],
                      canonical_scheme(row.get("selection")
                                       or LEGACY_SELECTION),
                      row.get("weakening") or LEGACY_WEAKENING,
                      row.get("metric") or LEGACY_METRIC,
                      row.get("repair_mode") or LEGACY_REPAIR,
                      row["spec"], int(row["seed"])))
    return done


def existing_fieldnames(csv_path: Path) -> list | None:
    """Header of an existing results CSV, or None if absent/empty."""
    if not csv_path.exists():
        return None
    with open(csv_path, newline="") as f:
        return next(csv.reader(f), None)


def append_row(csv_path: Path, row: dict, fieldnames: list) -> None:
    write_header = not csv_path.exists() or csv_path.stat().st_size == 0
    with open(csv_path, "a", newline="") as f:
        # extrasaction='ignore' drops columns (e.g. timed_out) that a legacy
        # CSV's header does not have, keeping appends compatible.
        writer = csv.DictWriter(f, fieldnames=fieldnames, extrasaction="ignore")
        if write_header:
            writer.writeheader()
        writer.writerow(row)


# ── Core runner ───────────────────────────────────────────────────────────────

def derive_config(config_path: Path, output_dir: Path, parallel_k: int) -> Path:
    """Write a copy of the level's TOML with `parallel = k` under [runtime].

    Caps counter's internal thread pool so that --jobs concurrent runs do not
    oversubscribe the machine (counter defaults to hardware_concurrency).
    """
    text = config_path.read_text()
    line = f"parallel = {parallel_k}"
    if re.search(r"(?m)^\s*parallel\s*=", text):
        text = re.sub(r"(?m)^\s*parallel\s*=.*$", line, text)
    elif re.search(r"(?m)^\[runtime\]\s*$", text):
        text = re.sub(r"(?m)^\[runtime\]\s*$", f"[runtime]\n{line}", text, count=1)
    else:
        text = text.rstrip("\n") + f"\n\n[runtime]\n{line}\n"
    derived = output_dir / "config.toml"
    derived.write_text(text)
    return derived


def run_one(config_path: Path, sweep: str, level_name: str, spec_name: str,
            seed: int, timeout: int, results_dir: Path, parallel_k=None,
            run_id: str | None = None,
            compare_timeout: int = COMPARE_TIMEOUT_S) -> dict | None:
    """Execute counter (+ compare) once; return the metric columns.

    The returned dict carries spec/seed and all metric fields but no
    sweep/level/selection columns — the caller stamps those per emitted
    (alias) row.

    `run_id` names the output directory and must be unique per executed run;
    the caller passes one that includes every crossed factor (selection scheme,
    and weakening state where the profile crosses it), since the same
    (sweep, level, spec, seed) is run once per factor combination and two of
    them would otherwise share a directory and read each other's repair_*.json.
    `run_id` does not encode the operating point, so profiles that differ only
    in that must pass different `results_dir`s for the same reason.
    """
    spec = SPECS[spec_name]

    if run_id is None:
        run_id = f"sweep_{sweep}_{level_name}_{spec_name}_seed{seed:02d}"
    output_dir = results_dir / run_id
    output_dir.mkdir(parents=True, exist_ok=True)

    effective_config = config_path
    if parallel_k is not None:
        effective_config = derive_config(config_path, output_dir, parallel_k)

    cmd = [
        str(COUNTER_BIN),
        "--input", str(spec["input"]),
        "--output-dir", str(output_dir),
        "--config", str(effective_config),
        "--seed", str(seed),
    ]

    log_path = output_dir / "run.log"

    t_start = time.monotonic()
    timed_out = False
    with open(log_path, "wb") as log_file:
        try:
            subprocess.run(
                cmd, check=True, timeout=timeout,
                stdout=log_file, stderr=subprocess.STDOUT,
            )
        except subprocess.TimeoutExpired:
            timed_out = True
            print(f"    [{run_id}] TIMEOUT after {timeout}s  (see {log_path})")
        except subprocess.CalledProcessError as e:
            log_file.flush()
            ctx = tail_line(log_path)
            print(f"    [{run_id}] ERROR: counter exited {e.returncode}"
                  f"  (see {log_path})" + (f"\n           {ctx}" if ctx else ""))
            return None
    wall = round(time.monotonic() - t_start, 2)

    n_repairs, best_fitness = parse_repair_files(output_dir)

    base = {
        "spec": spec_name,
        "seed": seed,
        "found_repair": int(n_repairs > 0),
        "n_repairs": n_repairs,
        "best_fitness": "" if math.isnan(best_fitness) else round(best_fitness, 6),
        "wall_time_s": wall,
        "timed_out": int(timed_out),
        "compare_timed_out": 0,
        "n_dropped": parse_dropped(log_path),
    }

    if n_repairs == 0 or timed_out:
        return {**base, "best_relation": "none", "implies_ideal": 0, "n_implies": 0}

    # compare's cost scales with n_repairs * n_ideals, so an arm that produces
    # more repairs hits the timeout more often. Scoring that as implies_ideal = 0
    # would read a *slower* comparison as a *worse* repair and bias the
    # comparison against exactly the arm under test, so the timeout is recorded
    # in its own column for the analysis to exclude rather than folded into the
    # response. A compare that failed for any other reason keeps the old
    # behaviour: it is a broken run, not a censored measurement.
    compare_timed_out = False
    try:
        result = subprocess.run(
            [str(COMPARE_BIN), "--repairs", str(output_dir),
             "--ideals", str(spec["ideals_dir"])],
            check=True, timeout=compare_timeout, capture_output=True, text=True,
        )
        best_rel, implies_ideal, n_implies = parse_compare_output(result.stdout)
    except subprocess.TimeoutExpired:
        print(f"    [{run_id}] WARN: compare timed out after {compare_timeout}s "
              f"on {n_repairs} repairs — recorded, not scored")
        compare_timed_out = True
        best_rel, implies_ideal, n_implies = "unknown", 0, 0
    except subprocess.CalledProcessError as e:
        print(f"    [{run_id}] WARN: compare failed — {e}")
        best_rel, implies_ideal, n_implies = "unknown", 0, 0

    return {**base, "best_relation": best_rel, "implies_ideal": implies_ideal,
            "n_implies": n_implies, "compare_timed_out": int(compare_timed_out)}


# ── Entry point ───────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--profile", choices=list(PROFILES), default="full",
                        help="Experiment profile (default: full)")
    parser.add_argument("--jobs", type=int, metavar="N",
                        help="Concurrent runs (default: per profile)")
    parser.add_argument("--sweeps", nargs="+", metavar="SWEEP",
                        help="Sweeps to run (default: per profile)")
    parser.add_argument("--specs", nargs="+", choices=list(SPECS),
                        metavar="SPEC",
                        help="Specs to run (default: per profile)")
    parser.add_argument("--seeds", nargs="+", type=int, metavar="N",
                        help="Seeds to run (default: per profile)")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--no-resume", action="store_true",
                        help="Re-run even if a result already exists in the CSV")
    parser.add_argument("--allow-stale-binary", action="store_true",
                        help="Warn instead of refusing when the binaries were "
                             "not built from the working tree's HEAD")
    args = parser.parse_args()

    profile = PROFILES[args.profile]
    jobs = args.jobs if args.jobs else profile["default_jobs"]
    if jobs < 1:
        sys.exit("--jobs must be >= 1")
    specs = args.specs or profile["specs"]
    seeds = args.seeds if args.seeds is not None else profile["seeds"]
    results_csv: Path = profile["results_csv"]
    configs_dir: Path = profile["configs_dir"]
    results_dir: Path = profile["results_dir"]

    # Validate binaries (skipped for --dry-run so plans work without a build)
    head = working_tree_head()
    unknown_version = {"commit": LEGACY_COMMIT, "commit_short": LEGACY_COMMIT,
                       "dirty": ""}
    versions = {"counter": dict(unknown_version),
                "compare": dict(unknown_version)}
    if not args.dry_run:
        for bin_path in [COUNTER_BIN, COMPARE_BIN]:
            if not bin_path.exists():
                sys.exit(
                    f"Binary not found: {bin_path}\n"
                    f"Run: cmake --workflow --preset release"
                )
        # Once, at startup: every run of this campaign uses these binaries, so
        # asking per run would cost thousands of subprocesses to learn one fact.
        versions = {"counter": binary_version(COUNTER_BIN),
                    "compare": binary_version(COMPARE_BIN)}

    def version_label(name: str) -> str:
        version = versions[name]
        suffix = "-dirty" if version.get("dirty") == "1" else ""
        return f"{name} {version['commit_short']}{suffix}"

    print("=" * 64)
    print(f"  Profile: {args.profile}")
    print(f"    jobs:    {jobs}")
    print(f"    specs:   {', '.join(specs)}")
    print(f"    seeds:   {min(seeds)}-{max(seeds)} ({len(seeds)} seeds)")
    print(f"    configs: {configs_dir}")
    print(f"    runs:    {results_dir}")
    print(f"    results: {results_csv}")
    print(f"    binary:  {version_label('counter')}, "
          f"{version_label('compare')}")
    print("=" * 64)

    if not args.dry_run:
        enforce_binary_freshness(versions, head, args.allow_stale_binary)

    # Collect config files
    if not configs_dir.exists():
        sys.exit(
            f"No configs found at {configs_dir}\n"
            f"Run: python scripts/gen_configs.py"
        )
    def _config_sort_key(p: Path):
        sweep, _, level_value = extract_metadata(p)
        numeric = isinstance(level_value, (int, float))
        return (scheme_of(p), weakening_of(p), metric_of(p), repair_mode_of(p),
                sweep, 0 if numeric else 1, level_value if numeric else 0,
                str(p))

    wanted_schemes = profile["schemes"]
    wanted_weakenings = profile["weakenings"]
    wanted_metrics = profile["metrics"]
    wanted_repairs = profile["repair_modes"]
    # Build the directory list by nesting each crossed factor a level deeper:
    # <scheme>/[<weakening>/][<metric>/][<repair>/]. A None factor means the
    # profile predates it and its segment is skipped, keeping the flat layout
    # readable.
    config_dirs = []
    for s in wanted_schemes:
        for w in (wanted_weakenings or [None]):
            for m in (wanted_metrics or [None]):
                for r in (wanted_repairs or [None]):
                    d = configs_dir / s
                    for seg in (w, m, r):
                        if seg is not None:
                            d = d / seg
                    config_dirs.append(d)
    all_configs = sorted(
        (c for d in config_dirs for c in d.glob("sweep_*.toml")),
        key=_config_sort_key)
    if not all_configs:
        sys.exit(
            f"No configs found under {configs_dir} for scheme(s) "
            f"{', '.join(wanted_schemes)}\nRun: python scripts/gen_configs.py"
        )
    configs_by_key = {
        (scheme_of(c), weakening_of(c), metric_of(c), repair_mode_of(c),
         *extract_metadata(c)[:2]): c
        for c in all_configs}

    if args.sweeps:
        wanted_sweeps = {s.upper() for s in args.sweeps}
    elif profile["sweeps"] is not None:
        wanted_sweeps = set(profile["sweeps"])
    else:
        wanted_sweeps = None  # every sweep found

    def selected(cfg: Path) -> bool:
        sweep, level_name, _ = extract_metadata(cfg)
        if wanted_sweeps is not None and sweep not in wanted_sweeps:
            return False
        allowed = profile["levels"].get(sweep)
        return allowed is None or level_name in allowed

    selected_configs = [c for c in all_configs if selected(c)]
    if not selected_configs:
        sys.exit("No matching config files found.")

    # Aliasing holds within one (scheme, weakening, metric, repair) cell:
    # wkoff/log's D/ptrig0.5 aliases onto wkoff/log's C/default, never onto
    # another cell's. The byte-identity check enforces it, since the configs
    # differ on the factor keys.
    # canonical_scheme, because configs_by_key is keyed by scheme_of(), which
    # canonicalises: a profile pointed at a config tree with the old directory
    # names would otherwise build its alias table under a key the lookup below
    # never asks for.
    factor_cells = [(canonical_scheme(s), w, m, r) for s in wanted_schemes
                    for w in (wanted_weakenings or [LEGACY_WEAKENING])
                    for m in (wanted_metrics or [LEGACY_METRIC])
                    for r in (wanted_repairs or [LEGACY_REPAIR])]
    active_aliases = {
        (s, w, m, r): verify_aliases(configs_by_key, s, w, m, r,
                                     profile["baseline_aliases"])
        for s, w, m, r in factor_cells}

    done = set() if args.no_resume else load_done_set(results_csv)

    # Build the plan as the set of desired result rows, grouped by the
    # canonical run that produces them: (sweep, level, spec, seed) →
    # list of (sweep, level_name) rows to emit (aliases share one execution).
    runs: dict[tuple, list] = {}
    n_rows = n_aliased = 0
    for cfg in selected_configs:
        scheme, weakening, metric, repair = (
            scheme_of(cfg), weakening_of(cfg), metric_of(cfg),
            repair_mode_of(cfg))
        sweep, level_name, _ = extract_metadata(cfg)
        canon = active_aliases[(scheme, weakening, metric, repair)].get(
            (sweep, level_name), (sweep, level_name))
        for spec_name in specs:
            for seed in seeds:
                n_rows += 1
                if canon != (sweep, level_name):
                    n_aliased += 1
                key = (scheme, weakening, metric, repair, canon[0], canon[1],
                       spec_name, seed)
                runs.setdefault(key, []).append((sweep, level_name))

    def row_key(key: tuple, sweep: str, level_name: str) -> tuple:
        scheme, weakening, metric, repair, _, _, spec_name, seed = key
        return (sweep, level_name, scheme, weakening, metric, repair,
                spec_name, seed)

    n_done = sum(
        1
        for key, row_list in runs.items()
        for sweep, level_name in row_list
        if row_key(key, sweep, level_name) in done
    )
    # Seed-major, so that killing the run at a wall-clock deadline leaves a
    # balanced design: every level sampled at the same seeds, just fewer of
    # them. The natural (config-major) order would instead finish the first
    # levels and leave the last ones at zero seeds, which is not analysable.
    #
    # Within a seed the order is spec, then the factor cell, so the arms of a
    # comparison run adjacently: at any kill point two cells of the same spec
    # and seed differ by at most one run. A profile whose factor is the thing
    # under test depends on that ordering for its design to survive an early
    # stop, not merely on its seeds being disjoint across hosts.
    def execution_order(key: tuple) -> tuple:
        (scheme, weakening, metric, repair, c_sweep, c_level,
         spec_name, seed) = key
        return (seed, spec_name, scheme, weakening, metric, repair,
                c_sweep, c_level)

    to_execute = sorted(
        (key for key, row_list in runs.items()
         if any(row_key(key, s, l) not in done for s, l in row_list)),
        key=execution_order,
    )
    print(f"Plan: {n_rows} result rows ({n_aliased} via aliasing), "
          f"{n_done} already done; {len(to_execute)} runs to execute")

    if args.dry_run:
        # Same order as `to_execute`. Printing the dict's config-major
        # insertion order instead would show every run of the first factor cell
        # before any of the second, which is what the ordering above exists to
        # avoid -- so the plan a launch is checked against would misreport the
        # one property (balance under an early stop) it is read to confirm.
        for key, row_list in sorted(runs.items(),
                                    key=lambda kv: execution_order(kv[0])):
            (scheme, weakening, metric, repair, c_sweep, c_level,
             spec_name, seed) = key
            for sweep, level_name in row_list:
                tags = []
                if (sweep, level_name) != (c_sweep, c_level):
                    tags.append(f"(alias of {c_sweep}/{c_level})")
                if row_key(key, sweep, level_name) in done:
                    tags.append("(skip)")
                # Names the config as it sits on disk, so a flat profile shows
                # no factor segment even though its rows record the defaults.
                cfg_dir = "/".join(
                    p for p in (scheme,
                                None if wanted_weakenings is None else weakening,
                                None if wanted_metrics is None else metric,
                                None if wanted_repairs is None else repair)
                    if p is not None)
                print(f"  {cfg_dir}/sweep_{sweep}_{level_name}"
                      f"  spec={spec_name}  seed={seed:02d}"
                      + ("  " + " ".join(tags) if tags else ""))
        return

    results_dir.mkdir(parents=True, exist_ok=True)
    results_csv.parent.mkdir(parents=True, exist_ok=True)

    manifest = manifest_path(results_csv)
    write_manifest(manifest, args.profile, profile, versions, head, jobs,
                   specs, seeds, wanted_sweeps)
    print(f"Manifest: {manifest}")

    fieldnames = existing_fieldnames(results_csv) or CSV_FIELDS
    for column in ["timed_out", "compare_timed_out", "weakening", "metric",
                   "repair_mode", "commit", "dirty"]:
        if column not in fieldnames:
            print(f"Note: {results_csv.name} predates the {column} column; "
                  f"appending without it")

    # Cap each run's internal thread pool so jobs * parallel ≈ core count.
    parallel_k = max(1, (os.cpu_count() or 1) // jobs) if jobs > 1 else None
    if parallel_k is not None:
        print(f"Per-run counter thread pool capped at parallel = {parallel_k}")

    lock = threading.Lock()
    state = {"completed": 0, "errors": 0, "rows_written": 0}
    n_exec = len(to_execute)
    t0 = time.monotonic()

    def execute(key: tuple) -> None:
        (scheme, weakening, metric, repair, c_sweep, c_level,
         spec_name, seed) = key
        cfg = configs_by_key[
            (scheme, weakening, metric, repair, c_sweep, c_level)]
        caps = profile["timeout_caps"]
        timeout = (caps[spec_name] if caps
                   else counter_timeout(c_level, level_value_of(c_level)))
        # A factor state joins run_id only where the profile crosses it: adding
        # it unconditionally would rename every existing run directory of the
        # profiles that predate the factor, orphaning their results.
        wk_tag = "" if wanted_weakenings is None else f"_{weakening}"
        mx_tag = "" if wanted_metrics is None else f"_{metric}"
        rp_tag = "" if wanted_repairs is None else f"_{repair}"
        run_id = (f"sweep_{c_sweep}_{c_level}_{scheme}{wk_tag}{mx_tag}{rp_tag}"
                  f"_{spec_name}_seed{seed:02d}")
        with lock:
            print(f"[start]      {run_id}  (timeout {timeout}s)", flush=True)

        result = run_one(cfg, c_sweep, c_level, spec_name, seed,
                         timeout, results_dir, parallel_k, run_id,
                         profile.get("compare_timeout", COMPARE_TIMEOUT_S))

        with lock:
            state["completed"] += 1
            n = state["completed"]
            elapsed = time.monotonic() - t0
            eta = elapsed / n * (n_exec - n)
            if result is None:
                state["errors"] += 1
                print(f"[{n}/{n_exec}]  {run_id}  FAILED"
                      f"  ETA {eta/60:.1f}min", flush=True)
                return
            for sweep, level_name in runs[key]:
                if row_key(key, sweep, level_name) in done:
                    continue
                row = {**result, "sweep": sweep, "level_name": level_name,
                       "level_value": level_value_of(level_name),
                       "selection": scheme, "weakening": weakening,
                       "metric": metric, "repair_mode": repair,
                       # The abbreviated hash, since this is a column a human
                       # reads; the manifest beside the CSV keeps the full one.
                       "commit": versions["counter"]["commit_short"],
                       "dirty": versions["counter"].get("dirty", "")}
                append_row(results_csv, row, fieldnames)
                done.add(row_key(key, sweep, level_name))
                state["rows_written"] += 1
            print(f"[{n}/{n_exec}]  {run_id}  done in {result['wall_time_s']}s"
                  f"  ETA {eta/60:.1f}min", flush=True)

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        list(pool.map(execute, to_execute))

    elapsed_total = time.monotonic() - t0
    print(
        f"\nDone. {state['completed']} runs ({state['rows_written']} rows) "
        f"in {elapsed_total/60:.1f} min, {state['errors']} errors."
        f"\nResults: {results_csv}"
    )


if __name__ == "__main__":
    main()
