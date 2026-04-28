# Counter

Repairing unrealisable FRETish specifications.

Uses a genetic algorithm with a fitness function that takes into account whether the specification is realisable, and its syntactic and semantic similarity with the original specification's requirements. Semantic similarity is estimated with a heuristic that computes $k$-bounded counting of traces that satisfy requirements using transfer matrices.

## Build and Run

```sh
mkdir -p build
cmake -S . -B build --preset debug
cmake --build --parallel $(nproc) --preset debug
./build/counter
```

### Dependencies

| Dependency | Link |
|---|---|
| Ganak | https://github.com/meelgroup/ganak |

Ganak only has release binaries for Linux and MacOS, but its [`CMakeLists.txt`](https://github.com/meelgroup/ganak/blob/master/CMakeLists.txt) makes reference to Windows, so it may be possible to compile for Windows. Currently the Linux x86_64 binary is hardcoded into [`cmake/ganak.cmake`](cmake/ganak.cmake).

## Format, Lint, and Test

```sh
cmake --build --parallel $(nproc) --preset debug --target format lint tests
```
