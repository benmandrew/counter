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
cmake -S . -B build
cmake --build build
```

## Run

```sh
./build/counter
```

The demo prints the transfer matrix, weighted transfer matrix, and exact counts
for trace lengths `k = 1..5`.

For uniform propositional valuations, the executable currently reports:

- `immediately`: `3, 9, 27, 81, 243`
- `at the next timepoint`: `4, 12, 36, 108, 324`

The counting routine follows:

$$
\textbf{n}^T * \hat{T}^{k - 1} * \textbf{1}
$$

where $\hat{T}[s, s'] = T[s, s'] * n_s'$ scales each transition by the number of
valuations for the target state.

## Test

Unit tests live under `test/` and are registered with CTest.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Format

```sh
cmake -S . -B build
cmake --build build --target format
```
