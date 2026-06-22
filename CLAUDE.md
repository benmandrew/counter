# counter

C++17 genetic algorithm for repairing unrealizable FRETISH specifications, using bounded model counting (SPOT + Ganak) for semantic similarity.

## Build

Two presets — `debug` (ASAN+UBSAN, `build/`) and `release` (`build-release/`):

```sh
cmake --workflow --preset debug      # configure + build + test
cmake --workflow --preset release

# Incremental build only (after initial configure):
cmake --build build
cmake --build build-release
```

## Tests

```sh
ctest --preset debug                  # run all tests
ctest --preset debug -R syntactic     # run tests matching a regex
```

Test binaries land at `build/test/counter_tests`. The test framework uses `expect(bool, message)` and `fail(message)` from `test/test_support.hpp`; each test suite is a free function declared in `test/test_suite.hpp`.

## Lint & Format

```sh
cmake --build build --target lint          # cpplint + clang-tidy + cppcheck
cmake --build build --target lint-cpplint
cmake --build build --target lint-clang-tidy
cmake --build build --target lint-cppcheck

cmake --build build --target format        # apply clang-format in-place
cmake --build build --target format-ci     # dry-run (fails if unformatted)
```

## Docs

```sh
cmake --build build --target docs   # Doxygen + Sphinx (requires both installed)
```

Every header file in `include/` must have a corresponding `.rst` page under `docs/api/` and be listed in `docs/index.rst`. When adding a new header, add the page and toctree entry before committing.

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
4. Run `Config::generations` rounds of `evolve_generation`: crossover + mutation, score offspring in a thread pool, apply filters (false-trigger, dedup, optional weakening).
5. Collect the realizable survivors from the final population (re-checked with `black` + `ltlsynt`).
6. Apply final filters: dedup, then optional implication filter to keep only maximal specs.
7. Score, sort, and write each maximal spec to `<output-dir>/repair_N.json`.

## CLI reference

**`counter`** — genetic repair

| Flag | Description |
|---|---|
| `--input <spec.json>` | Input specification (required) |
| `--output-dir <dir>` | Directory for `repair_N.json` outputs (required, must exist) |
| `--seed <n>` | RNG seed for reproducible runs |
| `-h`, `--help` | Show help |

**`realize`** — realizability check

```
realize <spec.json> [<spec.json> ...]
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help |

One file: prints `REALIZABLE` or `UNREALIZABLE`. Multiple files: prints `<path>: REALIZABLE` (or `UNREALIZABLE`) per line.

**`compare`** — implication comparison of repairs vs ideals

| Flag | Description |
|---|---|
| `--repairs <dir>` | Directory of repair JSON files (required) |
| `--ideal <file>` | Ideal repair to compare against (repeatable, required) |
| `-h`, `--help` | Show help |

**`ltl`** — print LTL formulae for specification requirements

```
ltl <spec.json> [<spec.json> ...]
```

| Flag | Description |
|---|---|
| `-h`, `--help` | Show help |

One file: prints assumptions and guarantees with their LTL formulae. Multiple files: prefixes each block with the file path. Each requirement prints as `[assumption]`/`[guarantee] <string>` followed by `LTL: <formula>` if one is present.

## External tools

- `ltl2tgba`, `ltlsynt` — from SPOT, built from source via `cmake/spot.cmake`; located via the `SPOT_BIN_DIR` compile macro.
- `black` — LTL satisfiability checker (`black-sat`); found on `PATH` or downloaded/built via `cmake/black.cmake`; path passed as `BLACK_EXECUTABLE_PATH`.
- `ganak` — model counter; downloaded as a release binary via `cmake/ganak.cmake`; path passed as `GANAK_EXECUTABLE_PATH`.

## Project layout

```
include/
  crash/          — crash_handler.hpp
  filter/         — bloat.hpp, implication.hpp, implication_check.hpp
  fitness/        — syntactic_similarity, semantic_similarity, model_counter, transfer_matrix
  genetic/        — mutation, crossover, generation
  prop_formula.hpp
  requirement.hpp — Requirement, Specification, Timing variants, requirement_to_ltl
src/
  crash/          — crash_handler.cpp (SIGSEGV/SIGABRT/SIGFPE handler), signal_tracer.cpp
  fitness/
  genetic/
  prop_formula/   — core, parser, cnf, similarity, transform, dimacs_api
  runner/         — spot.cpp, ganak.cpp, black.cpp (external tool wrappers)
test/
  fitness/
  genetic/
  prop_formula/
  runner/
```

## Debugging crashes from `crashes/`

`crash_handler.cpp` installs a SIGSEGV/SIGABRT/SIGFPE handler that forks
`signal_tracer` (cpptrace) and writes `crashes/crash_<pid>_<timestamp>.log`,
including the `--seed` and full `Config` values needed to reproduce. Both the
`release` and `relwithdebinfo` presets build with `-DNDEBUG` (CMake's default
for those build types), so:

- `assert()` is a no-op in these binaries — a failure path guarded only by
  `assert` (e.g. a non-zero subprocess exit code, a parse precondition) does
  *not* abort; it silently continues with whatever garbage/default value
  resulted. When chasing a crash, treat every `assert` between the crash site
  and the actual root cause as a place where bad data could have been let
  through instead of being caught loudly.
- `build-release/` has no debug info, so its crash logs only show raw
  addresses (`at .../counter`, no function/line). Don't try to reverse the
  PIE load offset by hand — rebuild `relwithdebinfo` instead (it keeps
  `-DNDEBUG`, so timing/behavior stays close to release, but adds `-g`):

  ```sh
  cmake --workflow --preset relwithdebinfo
  ```

- Reproduce with the exact seed from the crash log's `Config:` block:

  ```sh
  ./build-relwithdebinfo/counter --seed <seed from log>
  ```

  This is usually deterministic even though the crash often surfaces inside a
  worker thread (`score_population`'s `std::async` pool) — the RNG seed
  drives which specifications get generated, and the bug is typically a data
  bug (an unhandled edge case), not a true data race.
- If it reproduces, the new `crashes/` log from the `relwithdebinfo` binary is
  now fully symbolized (function + file:line per frame, including inlined
  frames) — read that first.
- To inspect the actual values at the fault (not just the stack shape), run
  under `gdb -batch`, since the program crashes itself before you'd get a
  chance to attach:

  ```sh
  gdb -q -batch -ex "run --seed <seed>" -ex "print <expr>" -ex bt \
      --args ./build-relwithdebinfo/counter --seed <seed>
  ```

  Inspecting the live locals (e.g. an empty `std::vector` where the code
  assumed at least one element) pinpoints the precondition that was violated,
  which is usually faster than reasoning from the call chain alone.

## Key types

- `Timing` — `std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks, AfterTicks, Eventually>` (see `requirement.hpp`).
- `ConditionType` — `enum class { Trigger, Continual }` controlling how the condition activates a `Requirement`: rising-edge (Trigger) or at every timepoint where it holds (Continual).
- `Requirement` — holds `m_condition`, `m_response`, `m_timing`, `m_condition_type`, and the derived `m_ltl` string.
- `Formula` — propositional AST with `syntactic_similarity`, `rewrite_post_order`, `n_subformulae`.
- `TransferSystem` — weighted automaton transition matrix for bounded model counting.
- `Count` — `uint64_t` or `unsigned __int128` (selected at configure time via `COUNTER_USE_UINT128`).
