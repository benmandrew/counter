#!/usr/bin/env python3
"""Generate TOML config files for all experiment sweeps.

Writes one file per (selection scheme, sweep, level) to
experiments/configs/<scheme>/. Safe to re-run — existing files are overwritten.

The scheme lives in the directory rather than the filename because
run_experiments.py parses (sweep, level) out of the filename, and it reads the
scheme back off the parent directory. Every sweep is generated once per scheme,
which makes selection_scheme a factor of the design rather than a constant.

Usage:
    python scripts/gen_configs.py                       # the standard grid
    python scripts/gen_configs.py --schemes nsga2       # one scheme only
    python scripts/gen_configs.py --sweeps C D E        # specific sweeps
    python scripts/gen_configs.py --generations 40 --population-size 1000 \\
        --out-dir experiments/configs-cj-large          # a larger operating point
    python scripts/gen_configs.py --weakening both      # cross run_weakening in
    python scripts/gen_configs.py --metric both         # cross direct/log metric in
"""

import argparse
from pathlib import Path

CONFIGS_DIR = Path(__file__).parent.parent / "experiments" / "configs"

# Selection schemes the whole grid is duplicated across. config.hpp defaults to
# "weighted"; a config omitting the key silently gets that, which is why every
# generated config pins it explicitly.
SCHEMES: list[str] = ["nsga2", "weighted"]

# run_weakening as a crossed factor: each (scheme, sweep, level) is emitted once
# per weakening state into its own <scheme>/<wkon|wkoff>/ directory. The state
# lives in the directory for the same reason the scheme does — level_value_of()
# in run_experiments.py parses a trailing number off level_name, so folding it
# into the filename would corrupt the level value.
#
# --weakening defaults to None rather than a state, because passing it is what
# introduces the extra directory level. Without it the layout stays flat
# (<scheme>/sweep_X_level.toml) and run_weakening comes from DEFAULTS or the
# level's own override, which is what keeps the no-arg output byte-identical to
# the grids generated before this factor existed.
WEAKENINGS: dict[str, list[tuple[str, bool]]] = {
    "on":   [("wkon", True)],
    "off":  [("wkoff", False)],
    "both": [("wkon", True), ("wkoff", False)],
}

# model_counting.metric as a crossed factor, exactly like WEAKENINGS above.
# Each entry is (dir_label, toml_value): the directory + CSV label is the short
# "direct"/"log", but the TOML value must be the full "direct"/"logarithmic"
# string config_io.cpp's apply_model_counting accepts, or the run aborts. Like
# --weakening, --metric defaults to None so the flat layout and the metric from
# DEFAULTS stay byte-identical to the pre-factor grids.
METRICS: dict[str, list[tuple[str, str]]] = {
    "direct": [("direct", "direct")],
    "log":    [("log", "logarithmic")],
    "both":   [("direct", "direct"), ("log", "logarithmic")],
}

# Mirrors the built-in defaults from include/config.hpp, with one deliberate
# exception: config.hpp defaults selection_scheme to "weighted", but the
# baseline of every sweep is NSGA-II, so DEFAULTS pins "nsga2" and the
# generator overrides it per scheme.
DEFAULTS: dict = {
    "generations": 10,
    "population_size": 200,
    "selection_scheme": "nsga2",
    "crossover_rate": 0.1,
    "mutation_rate": 1.0,
    "weight_syntactic": 0.33,
    "weight_semantic": 0.33,
    "weight_halstead": 0.1,
    "weight_status": 0.33,
    "weight_trigger": 1.0,
    "weight_response": 1.0,
    "weight_timing": 1.0,
    "p_trigger": 0.5,
    "p_response": 0.5,
    "p_timing": 0.15,
    "default_bound": 20,
    "metric": "direct",
    "run_weakening": True,
    "run_implication": True,
    "black_timeout_ms": 1000,
}


def _fmt(v: object) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        s = f"{v:.6g}"
        return s if "." in s else s + ".0"
    return str(v)


def make_toml(overrides: dict, defaults: dict = DEFAULTS) -> str:
    # Per-level overrides win over `defaults`, so --generations/--population-size
    # shift the operating point of every sweep that leaves those keys alone
    # without flattening sweeps A/B, which vary exactly those keys per level.
    d = {**defaults, **overrides}
    return "\n".join([
        "[genetic]",
        f"generations     = {d['generations']}",
        f"population_size = {d['population_size']}",
        f'selection_scheme = "{d["selection_scheme"]}"',
        f"crossover_rate  = {_fmt(d['crossover_rate'])}",
        f"mutation_rate   = {_fmt(d['mutation_rate'])}",
        "",
        "[fitness]",
        f"weight_syntactic = {_fmt(d['weight_syntactic'])}",
        f"weight_semantic  = {_fmt(d['weight_semantic'])}",
        f"weight_halstead  = {_fmt(d['weight_halstead'])}",
        f"weight_status    = {_fmt(d['weight_status'])}",
        "",
        "[syntactic]",
        f"weight_trigger  = {_fmt(d['weight_trigger'])}",
        f"weight_response = {_fmt(d['weight_response'])}",
        f"weight_timing   = {_fmt(d['weight_timing'])}",
        "",
        "[mutation]",
        f"p_trigger  = {_fmt(d['p_trigger'])}",
        f"p_response = {_fmt(d['p_response'])}",
        f"p_timing   = {_fmt(d['p_timing'])}",
        "",
        "[model_counting]",
        f"default_bound = {d['default_bound']}",
        f'metric = "{d["metric"]}"',
        "",
        "[filters]",
        f"run_weakening   = {_fmt(d['run_weakening'])}",
        f"run_implication = {_fmt(d['run_implication'])}",
        "",
        "[runtime]",
        f"black_timeout_ms = {d['black_timeout_ms']}",
        "",
    ])


# ── Sweep definitions ────────────────────────────────────────────────────────

# Every sweep holds the other parameters at DEFAULTS, so exactly one level per
# sweep is byte-identical to the A/gen10 baseline. run_experiments.py aliases
# those onto the baseline run rather than executing them again, which is why
# each grid below includes its own default value as a level.

# Sweep A: vary generations (population_size fixed at 200)
SWEEP_A: list[tuple[str, dict]] = [
    ("gen5",  {"generations": 5}),
    ("gen10", {"generations": 10}),   # baseline
    ("gen15", {"generations": 15}),
    ("gen20", {"generations": 20}),
    ("gen30", {"generations": 30}),
    ("gen40", {"generations": 40}),
    ("gen60", {"generations": 60}),
    ("gen80", {"generations": 80}),
]

# Sweep B: vary population size (generations fixed at 10)
SWEEP_B: list[tuple[str, dict]] = [
    ("pop50",   {"population_size": 50}),
    ("pop75",   {"population_size": 75}),
    ("pop100",  {"population_size": 100}),
    ("pop150",  {"population_size": 150}),
    ("pop200",  {"population_size": 200}),   # baseline
    ("pop300",  {"population_size": 300}),
    ("pop500",  {"population_size": 500}),
    ("pop750",  {"population_size": 750}),
    ("pop1000", {"population_size": 1000}),
    ("pop1500", {"population_size": 1500}),
]

# Sweep C: vary fitness weight presets (generations=10, population_size=200)
SWEEP_C: list[tuple[str, dict]] = [
    ("default",         {}),
    ("syntactic-heavy", {"weight_syntactic": 0.8, "weight_semantic": 0.2}),
    ("semantic-heavy",  {"weight_syntactic": 0.2, "weight_semantic": 0.8}),
    ("status-only",     {"weight_syntactic": 0.0, "weight_semantic": 0.0,
                         "weight_halstead": 0.0, "weight_status": 1.0}),
    ("no-halstead",     {"weight_syntactic": 0.33, "weight_semantic": 0.5,
                         "weight_halstead": 0.0,  "weight_status": 0.5}),
]

# Sweep D: vary the trigger mutation probability
SWEEP_D: list[tuple[str, dict]] = [
    ("ptrig0.0",  {"p_trigger": 0.0}),
    ("ptrig0.1",  {"p_trigger": 0.1}),
    ("ptrig0.25", {"p_trigger": 0.25}),
    ("ptrig0.5",  {"p_trigger": 0.5}),   # baseline
    ("ptrig0.75", {"p_trigger": 0.75}),
    ("ptrig0.9",  {"p_trigger": 0.9}),
    ("ptrig1.0",  {"p_trigger": 1.0}),
]

# Sweep E: vary the response mutation probability
SWEEP_E: list[tuple[str, dict]] = [
    ("presp0.0",  {"p_response": 0.0}),
    ("presp0.1",  {"p_response": 0.1}),
    ("presp0.25", {"p_response": 0.25}),
    ("presp0.5",  {"p_response": 0.5}),  # baseline
    ("presp0.75", {"p_response": 0.75}),
    ("presp0.9",  {"p_response": 0.9}),
    ("presp1.0",  {"p_response": 1.0}),
]

# Sweep F: vary the timing mutation probability
SWEEP_F: list[tuple[str, dict]] = [
    ("ptim0.0",  {"p_timing": 0.0}),
    ("ptim0.05", {"p_timing": 0.05}),
    ("ptim0.15", {"p_timing": 0.15}),   # baseline
    ("ptim0.3",  {"p_timing": 0.3}),
    ("ptim0.5",  {"p_timing": 0.5}),
    ("ptim0.75", {"p_timing": 0.75}),
    ("ptim1.0",  {"p_timing": 1.0}),
]

# Sweep G: vary the bounded model-counting horizon. Raising it costs almost
# nothing — the bound enters through the transfer matrix, not a SAT call, and
# bound 80 measured within noise of bound 5 — but it moves the semantic
# similarity score, so it changes which repairs win.
SWEEP_G: list[tuple[str, dict]] = [
    ("bound5",   {"default_bound": 5}),
    ("bound10",  {"default_bound": 10}),
    ("bound20",  {"default_bound": 20}),   # baseline
    ("bound40",  {"default_bound": 40}),
    ("bound80",  {"default_bound": 80}),
    ("bound160", {"default_bound": 160}),
]

# Sweep H: vary the crossover rate. The default of 0.1 leaves the search
# almost entirely mutation-driven; the 0.0 level tests whether crossover
# contributes at all.
SWEEP_H: list[tuple[str, dict]] = [
    ("cross0.0",  {"crossover_rate": 0.0}),
    ("cross0.1",  {"crossover_rate": 0.1}),  # baseline
    ("cross0.25", {"crossover_rate": 0.25}),
    ("cross0.5",  {"crossover_rate": 0.5}),
    ("cross0.75", {"crossover_rate": 0.75}),
    ("cross1.0",  {"crossover_rate": 1.0}),
]

# Sweep I: vary the mutation rate
SWEEP_I: list[tuple[str, dict]] = [
    ("mut0.1",  {"mutation_rate": 0.1}),
    ("mut0.25", {"mutation_rate": 0.25}),
    ("mut0.5",  {"mutation_rate": 0.5}),
    ("mut0.75", {"mutation_rate": 0.75}),
    ("mut1.0",  {"mutation_rate": 1.0}),   # baseline
]

# Sweep J: ablate the weakening filter
SWEEP_J: list[tuple[str, dict]] = [
    ("weaken-on",  {"run_weakening": True}),   # baseline
    ("weaken-off", {"run_weakening": False}),
]

SWEEPS: list[tuple[str, list]] = [
    ("A", SWEEP_A),
    ("B", SWEEP_B),
    ("C", SWEEP_C),
    ("D", SWEEP_D),
    ("E", SWEEP_E),
    ("F", SWEEP_F),
    ("G", SWEEP_G),
    ("H", SWEEP_H),
    ("I", SWEEP_I),
    ("J", SWEEP_J),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--generations", type=int, default=DEFAULTS["generations"],
                        metavar="N",
                        help=f"Baseline generations (default: "
                             f"{DEFAULTS['generations']}); sweep A overrides it "
                             f"per level")
    parser.add_argument("--population-size", type=int,
                        default=DEFAULTS["population_size"], metavar="N",
                        help=f"Baseline population size (default: "
                             f"{DEFAULTS['population_size']}); sweep B overrides "
                             f"it per level")
    parser.add_argument("--schemes", nargs="+", choices=SCHEMES, default=SCHEMES,
                        metavar="SCHEME",
                        help=f"Selection schemes to emit (default: "
                             f"{' '.join(SCHEMES)})")
    parser.add_argument("--sweeps", nargs="+", choices=[n for n, _ in SWEEPS],
                        default=[n for n, _ in SWEEPS], metavar="SWEEP",
                        help="Sweeps to emit (default: all)")
    parser.add_argument("--weakening", choices=list(WEAKENINGS), default=None,
                        metavar="STATE",
                        help="Cross run_weakening in as a factor, writing "
                             "<scheme>/<wkon|wkoff>/ (choices: "
                             f"{', '.join(WEAKENINGS)}). Omit to keep the flat "
                             "layout and take run_weakening from the defaults")
    parser.add_argument("--metric", choices=list(METRICS), default=None,
                        metavar="METRIC",
                        help="Cross model_counting.metric in as a factor, "
                             "writing <scheme>/[<weakening>/]<direct|log>/ "
                             f"(choices: {', '.join(METRICS)}). Omit to keep the "
                             "flat layout and take metric from the defaults")
    parser.add_argument("--out-dir", type=Path, default=CONFIGS_DIR, metavar="PATH",
                        help=f"Directory to write <scheme>/ dirs into (default: "
                             f"{CONFIGS_DIR})")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    defaults = {**DEFAULTS,
                "generations": args.generations,
                "population_size": args.population_size}
    wanted = set(args.sweeps)
    sweeps = [(name, levels) for name, levels in SWEEPS if name in wanted]
    # (subdirectory, run_weakening override). The flat case carries no override,
    # so sweep J's per-level run_weakening still reaches the emitted TOML.
    weakenings: list[tuple[str | None, dict]] = (
        [(d, {"run_weakening": v}) for d, v in WEAKENINGS[args.weakening]]
        if args.weakening else [(None, {})]
    )
    # (subdirectory, metric override). None ⇒ flat layout with the metric from
    # DEFAULTS, mirroring the weakening default. The metric directory nests
    # below the weakening one: <scheme>/[<weakening>/]<direct|log>/.
    metrics: list[tuple[str | None, dict]] = (
        [(d, {"metric": v}) for d, v in METRICS[args.metric]]
        if args.metric else [(None, {})]
    )

    count = 0
    for scheme in args.schemes:
        for wk_dir, wk_override in weakenings:
            for mx_dir, mx_override in metrics:
                out = args.out_dir / scheme
                if wk_dir is not None:
                    out = out / wk_dir
                if mx_dir is not None:
                    out = out / mx_dir
                out.mkdir(parents=True, exist_ok=True)
                for sweep_name, levels in sweeps:
                    for level_name, overrides in levels:
                        path = out / f"sweep_{sweep_name}_{level_name}.toml"
                        path.write_text(make_toml(
                            {**overrides, "selection_scheme": scheme,
                             **wk_override, **mx_override},
                            defaults))
                        count += 1
                label = "/".join(p for p in (scheme, wk_dir, mx_dir)
                                 if p is not None)
                print(f"  {label:20} "
                      f"{len(list(out.glob('sweep_*.toml'))):3} configs")
    print(f"\nGenerated {count} config files in {args.out_dir}")


if __name__ == "__main__":
    main()
