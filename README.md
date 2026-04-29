# Counter

Repairing unrealisable FRETish specifications.

Uses a genetic algorithm with a fitness function that takes into account whether the specification is realisable, and its syntactic and semantic similarity with the original specification's requirements. Semantic similarity is estimated with a heuristic that computes $k$-bounded counting of traces that satisfy requirements using transfer matrices.

## Build and Run

```sh
cmake --preset debug
cmake --build --preset debug
./build/counter
```

Pass `--parallel N` to `cmake --build` to build with N parallel jobs.

### Dependencies

| Dependency | Link | Platforms |
|---|---|---|
| Ganak | https://github.com/meelgroup/ganak | Linux (x86_64), macOS — [`cmake/ganak.cmake`](cmake/ganak.cmake) downloads the Linux x86_64 binary |
| Spot | https://spot.lre.epita.fr | Linux, macOS — built from source via [`cmake/spot.cmake`](cmake/spot.cmake) |
| Black | https://www.black-sat.org | Linux — detected via `find_program`; if not found, [`cmake/black.cmake`](cmake/black.cmake) downloads the Ubuntu 24.04 x86_64 package, which requires `libfmt9` to be installed separately (`sudo apt-get install libfmt-dev`) |

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
