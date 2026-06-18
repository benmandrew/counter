# Counter

Repairing unrealisable FRETish specifications.

Uses a genetic algorithm with a fitness function that takes into account whether the specification is realisable, and its syntactic and semantic similarity with the original specification's requirements. Semantic similarity is estimated with a heuristic that computes $k$-bounded counting of traces that satisfy requirements using transfer matrices.

## Build

```sh
cmake --preset debug
cmake --build --preset debug
```

Pass `--parallel N` to `cmake --build` to build with N parallel jobs.

## CLI Reference

### `counter` — repair an unrealisable specification

```
counter --input <spec.json> --output-dir <dir> [--seed <n>]
```

Runs the genetic algorithm and writes the maximal realisable repairs to
`<dir>/repair_0.json`, `repair_1.json`, … Requires the output directory to
already exist. Use `--seed` for reproducible runs; the chosen seed is printed
at startup.

### `compare` — compare repairs against ideal specifications

```
compare --repairs <dir> --ideal <file> [--ideal <file>...]
```

For each `.json` file in `<dir>`, checks the implication relation against every
`--ideal` file and prints whether the repair is equivalent to, strictly stronger
than, strictly weaker than, or incomparable with the ideal under the
assume-guarantee order.

### `realize` — check whether a specification is realisable

```
realize --input <spec.json>
```

Prints `REALIZABLE` or `UNREALIZABLE` to stdout and exits 0 on success, 1 on
error. Useful for quickly validating a single spec without running the full
repair loop.

### Dependencies

| Dependency | Link | Platforms |
|---|---|---|
| Ganak | https://github.com/meelgroup/ganak | Linux (x86\_64, ARM64), macOS (x86\_64, ARM64) — [`cmake/ganak.cmake`](cmake/ganak.cmake) downloads the appropriate pre-built binary |
| Spot | https://spot.lre.epita.fr | Linux, macOS — built from source via [`cmake/spot.cmake`](cmake/spot.cmake) |
| Black | https://www.black-sat.org | Linux (x86\_64), macOS (x86\_64, ARM64) — detected via `find_program`; if not found, [`cmake/black.cmake`](cmake/black.cmake) downloads the Ubuntu 24.04 x86\_64 package on Linux (requires `libfmt9`: `sudo apt-get install libfmt-dev`), or builds from source on macOS (requires Homebrew for `fmt`) |

## Format, Lint, and Test

```sh
cmake --build --preset debug --target format lint tests
```

## Build Documentation

```sh
cmake --build --preset debug --target docs
```

Depends on Doxygen and Sphinx.

## Configure, Build, and Test in One Step

```sh
cmake --workflow --preset debug
```
