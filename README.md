# Counter

Small C++ project for k-bounded counting of traces that satisfy simple FRET-like
requirements using transfer matrices.

The current implementation supports propositional trigger/response requirements of
the form `when P, C shall TIMING satisfy Q` for these timing modes:

- `immediately`
- `at the next timepoint`

The executable is linked against a static library built from functionality split
across:

- `include/requirement.hpp` and `src/requirement.cpp`
- `include/transfer_matrix.hpp` and `src/transfer_matrix.cpp`
- `include/model_counter.hpp` and `src/model_counter.cpp`

## Build

```sh
cmake -S . -B build/debug
cmake --build build/debug
```

## Run

```sh
./build/debug/counter
```

The demo prints the transfer matrix, weighted transfer matrix, and exact counts
for trace lengths `k = 1..5`.

For uniform propositional valuations, the executable currently reports:

- `immediately`: `3, 9, 27, 81, 243`
- `at the next timepoint`: `4, 12, 36, 108, 324`

The counting routine follows:

```text
n^T * T_hat^(k - 1) * 1
```

where `T_hat[s, s'] = T[s, s'] * n_s'` scales each transition by the number of
valuations for the target state.

## Format

```sh
cmake -S . -B build/debug
cmake --build build/debug --target format
```
