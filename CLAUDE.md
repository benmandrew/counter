# counter

C++17 genetic algorithm for repairing unrealizable FRETISH specifications, using bounded model counting (SPOT + Ganak) for semantic similarity.

## Build

Enter the dev shell first (Nix is the primary workflow):

```sh
nix develop
```

Two presets — `debug` (ASAN+UBSAN, `build/`) and `release` (`build-release/`):

```sh
cmake --workflow --preset debug      # configure + build + test
cmake --workflow --preset release

# Incremental build only (after initial configure):
cmake --build build
cmake --build build-release
```

Non-Nix: requires CMake ≥ 3.25, Ninja (the presets' generator), a C++17 compiler, `libunwind`, and Node.js (runs the vendored FRET formaliser CLI, not fetched by CMake); everything else is fetched by CMake.

## Tests

```sh
ctest --preset debug                  # run all tests
ctest --preset debug -R syntactic     # run tests matching a regex
```

Test binaries land at `build/test/counter_tests`. The test framework uses `expect(bool, message)` and `fail(message)` from `test/test_support.hpp`; each test suite is a free function declared in `test/test_suite.hpp`.

The dashboard page's script is tested separately, under node's built-in runner
(`test/web/*.test.mjs`, registered with ctest as `dashboard_page`; run directly
with `node --test "test/web/*.test.mjs"`). `test/web/harness.mjs` extracts the
`<script>` block from `web/dashboard.html` and evaluates it, so the tests run
against the page that actually ships rather than a copy of it. CMake skips them
when `node` is absent.

## Lint & Format

```sh
cmake --build build --target lint          # cpplint + clang-tidy + cppcheck + config parity
cmake --build build --target lint-cpplint
cmake --build build --target lint-clang-tidy
cmake --build build --target lint-cppcheck
cmake --build build --target lint-config-schema

cmake --build build --target format        # apply clang-format in-place
cmake --build build --target format-ci     # dry-run (fails if unformatted)
```

The same checks run from `.githooks/pre-commit`, which is tracked. Git refuses to clone hooks, so `core.hooksPath` has to be set locally once per clone; `cmake/githooks.cmake` does it at configure time, since the hooks shell out to build targets and cannot work before that anyway. Edit the hook in `.githooks/`, not in `.git/hooks/`, which git no longer reads once the path is set. Bypass a hook with `git commit --no-verify`; CI is the real enforcement.

## Config keys

Adding a TOML config key means editing three places, none of which the compiler ties together: the `apply_*` function in `src/config_io.cpp` that reads it, `config_key_spec()` in the same file (else the parser warns "unknown key" on a key it accepts), and `schemas/config-schema.json` (else editors reject it). `scripts/check_config_schema.py` enforces the last two against each other and against `example-config.toml`, and runs as part of `lint`.

## Docs

Every header file in `include/` must have a corresponding `.rst` page under `docs/api/` and be listed in `docs/index.rst`. When adding a new header, add the page and toctree entry before committing. The internal reference needs no per-file upkeep — it scans `src/` automatically, so nothing extra is required there.

## Code style

- **Standard**: C++17, `-Wall -Wextra -Wpedantic -Werror` on all targets.
- **Formatting**: clang-format (`.clang-format` at repo root). Run `format` target before committing.
- **Linting**: clang-tidy (`.clang-tidy`), cpplint (`CPPLINT.cfg`), cppcheck (`cppcheck_suppressions.txt`).
- **Comments**: only when the WHY is non-obvious — no narrating what the code does, no docstrings repeating parameter names already clear from the signature.
- **Assertions**: use `assert()` for internal invariants; `throw` only at API boundaries.
- **Overflow**: arithmetic on `Count` values must go through `count_add_overflow` / `count_mul_overflow` with an assert on the overflow flag.
- **Visitor pattern**: prefer `std::visit` with `if constexpr` branches over chains of `std::get_if` when dispatching on `std::variant` (see `requirement_to_ltl`, `mutate_timing`).

## Algorithm flow (`counter`)

1. Load `Specification` from `--input` JSON.
2. Build `AggregateWeightedFitnessFunction` (syntactic + semantic + Halstead + status) and per-generation `FilterFunction` list from the original spec.
3. Seed an RNG (from `--seed` or `std::random_device`); register crash metadata.
4. Run `Config::generations` rounds of `evolve_generation`: crossover + mutation, score offspring in a thread pool, apply filters (false-trigger, dedup, optional weakening). Selection follows `Config::selection_scheme`: `Nsga2` (default) ranks by non-dominated sorting + crowding distance over the individual objectives (`include/genetic/nsga2.hpp`) with (μ+λ) survivor pooling; `Nsga2Replicate` ranks identically but deduplicates the pool before sorting and replicates the distinct survivors back to `population_size`, apportioning copies by `1 / (1 + rank)`; `WeightedAverage` ranks by the aggregate scalar, but converges prematurely and is kept for comparison rather than use. The scored population always carries both the per-objective vector and the weighted scalar (`Scored<Spec>`).
5. Collect the realizable survivors from the final population (re-checked with `black` + `ltlsynt`).
6. Apply final filters: dedup, then optional implication filter to keep only maximal specs.
7. Score, sort, and write each maximal spec to `<output-dir>/repair_N.json`.

## Live dashboard

Opt-in, via `counter --dashboard` or `[runtime] dashboard = true` (the flag can
only enable). Off by default so a campaign of many runs does not pay for the
file and its flushes with nobody watching. When on, both drivers stream progress
to `<output-dir>/progress.jsonl` (one JSON object per line, flushed as written)
and copy `web/dashboard.html` there as `index.html`. To watch a run:
`python3 -m http.server -d <output-dir> 8000`. The page polls once a second;
`?poll=<seconds>` overrides that (`?poll=0` loads once and stops polling).

Each `stage` record carries `distinct` beside `n_in`/`n_out`: how many of the
survivors are distinct specifications. The population is largely repeats, which
no size can show, so this is the field that measures whether a selection scheme
actually keeps diversity. Computing it hashes the whole population, so
`run_generation_pipeline` only does so when an observer is attached — a run
without the dashboard pays nothing.

The page's script keeps everything above its `boot()` call free of DOM access at
load time: `boot()` runs only when `document` exists, and otherwise the script
exports its functions for `test/web/` to test under node. Adding a top-level
`document.getElementById` (rather than one inside a function) breaks that and
takes the JS tests with it.

The page derives its stage list from the `stage` records of the latest
generation, so a new filter or pipeline stage shows up with no change to either
side. Generation stages come from `make_generation_pipeline`
(`include/genetic/pipeline.hpp`), which returns an ordered vector of named
`PipelineStage`s; `run_generation_pipeline` reports each to an optional
`StageObserver`. Breeding must stay a single stage — crossover and mutation
interleave per offspring slot, so splitting them reorders every RNG draw after
the first and breaks seed reproducibility. The `determinism` test suite pins the
draw stream against exactly that.

## TLSF repair modes

Binaries: `counter` (genetic repair), `realize`, `compare`, `ltl`, `mucs` — run each with `--help` for flags.

`mucs` extracts a minimal unrealizable core. Prints the smallest subset of the guarantee-side sections (PRESET, ASSERT, GUARANTEE) that stays unrealizable against the full, unchanged environment side (INITIALLY, REQUIRE, ASSUME) — the culprit formulae behind unrealizability. Uses QuickXplain over `ltlsynt`. Prints `REALIZABLE (no core)` if the input is already realizable. TLSF-only (FRETISH JSON is not supported).

The same core extraction drives an alternative TLSF **repair mode**. `Config::repair_mode` (TOML `[tlsf] repair_mode = "monolithic" | "muc"`, default `monolithic`) selects between evolving the whole spec at once and the MUC-guided loop in `run_muc` (`src/tlsf/pipeline.cpp`): extract a core, evolve only that sub-spec, reintegrate the repaired core with the untouched non-core guarantees (`tlsf::reintegrate`), and repeat until the whole spec is realizable or `muc_max_iterations` trips. FRETISH ignores it. `scripts/gen_configs.py --repair both` and the `muc` profile in `run_experiments.py` cross the two modes as an experiment factor over the TLSF spec corpus.

## External tools

- `ltl2tgba`, `ltlsynt` — from SPOT, built from source via `cmake/spot.cmake`; located via the `SPOT_BIN_DIR` compile macro.
- `black` — LTL satisfiability checker (`black-sat`); found on `PATH` or downloaded/built via `cmake/black.cmake`; path passed as `BLACK_EXECUTABLE_PATH`. **Always run with a timeout**: `black -t <seconds> ...`.
- `ganak` — model counter; downloaded as a release binary via `cmake/ganak.cmake`; path passed as `GANAK_EXECUTABLE_PATH`.
- `node` — runs the vendored FRET formaliser CLI (`vendor/fretCLI.main.js formalize --logic ft-inf --batch`, see `runner/formaliser.hpp`); looked up on `PATH` at run time, not built or fetched by CMake, so it must be installed separately (`nodejs` in `flake.nix`).

## Key types

- `Timing` — `std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks, AfterTicks, Eventually>` (see `requirement.hpp`).
- `ConditionType` — `enum class { Trigger, Continual }` controlling how the condition activates a `Requirement`: rising-edge (Trigger) or at every timepoint where it holds (Continual).
- `Requirement` — holds `m_condition`, `m_response`, `m_timing`, `m_condition_type`, the derived `m_ltl` string, and `m_weakenable`. When `m_weakenable` is false the requirement is locked: the genetic algorithm never mutates it, uses it as a crossover source, or simplifies it. It is part of `Requirement`'s identity (`operator<`/`==`/`hash`). Serialised as the optional JSON key `weakenable` (default `true`, emitted only when `false`).
- `Formula` — propositional AST with `syntactic_similarity`, `rewrite_post_order`, `n_subformulae`.
- `TransferSystem` — weighted automaton transition matrix for bounded model counting.
- `Count` — `long double` (x87 80-bit: integers exact to 2^64, exponent range to 2^16384). Trace counts are only consumed as ratios cast to `double`, so range matters more than exact integer width; `count_add_overflow`/`count_mul_overflow` report overflow as a non-finite result.
