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

## Project layout

```
include/
  fitness/        — syntactic_similarity, semantic_similarity, model_counter, transfer_matrix
  genetic/        — mutation, crossover, generation
  prop_formula.hpp
  requirement.hpp — Requirement, Specification, Timing variants, requirement_to_ltl
src/
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

## Key types

- `Timing` — `std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks, AfterTicks, Eventually>` (see `requirement.hpp`).
- `Formula` — propositional AST with `syntactic_similarity`, `rewrite_post_order`, `n_subformulae`.
- `TransferSystem` — weighted automaton transition matrix for bounded model counting.
- `Count` — `uint64_t` or `unsigned __int128` (selected at configure time via `COUNTER_USE_UINT128`).
