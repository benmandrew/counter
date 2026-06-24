#!/usr/bin/env python3
"""Generate TOML config files for all experiment sweeps.

Writes one file per (sweep, level) combination to experiments/configs/.
Safe to re-run — existing files are overwritten.
"""

from pathlib import Path

CONFIGS_DIR = Path(__file__).parent.parent / "experiments" / "configs"

# Mirrors the built-in defaults from include/config.hpp
DEFAULTS: dict = {
    "generations": 10,
    "population_size": 200,
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


def make_toml(overrides: dict) -> str:
    d = {**DEFAULTS, **overrides}
    return "\n".join([
        "[genetic]",
        f"generations     = {d['generations']}",
        f"population_size = {d['population_size']}",
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

# Sweep A: vary generations (population_size fixed at 200)
SWEEP_A: list[tuple[str, dict]] = [
    ("gen5",   {"generations": 5}),
    ("gen10",  {"generations": 10}),
    ("gen20",  {"generations": 20}),
    ("gen50",  {"generations": 50}),
    ("gen100", {"generations": 100}),
]

# Sweep B: vary population size (generations fixed at 10)
SWEEP_B: list[tuple[str, dict]] = [
    ("pop50",   {"population_size": 50}),
    ("pop100",  {"population_size": 100}),
    ("pop200",  {"population_size": 200}),
    ("pop500",  {"population_size": 500}),
    ("pop1000", {"population_size": 1000}),
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

SWEEPS: list[tuple[str, list]] = [
    ("A", SWEEP_A),
    ("B", SWEEP_B),
    ("C", SWEEP_C),
]


def main() -> None:
    CONFIGS_DIR.mkdir(parents=True, exist_ok=True)
    count = 0
    for sweep_name, levels in SWEEPS:
        for level_name, overrides in levels:
            filename = f"sweep_{sweep_name}_{level_name}.toml"
            path = CONFIGS_DIR / filename
            path.write_text(make_toml(overrides))
            print(f"  wrote {path.relative_to(Path.cwd())}")
            count += 1
    print(f"\nGenerated {count} config files in {CONFIGS_DIR}")


if __name__ == "__main__":
    main()
