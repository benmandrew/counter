# Counter

Repairing unrealisable FRETish specifications using a genetic algorithm.

## Build

### With Nix (recommended)

Requires [Nix](https://nixos.org/download/) with flakes enabled.

```sh
nix develop                          # enter dev shell (first run fetches dependencies)
cmake --workflow --preset debug      # configure + build + test
cmake --build build                  # incremental build only
```

A `.envrc` (`use flake`) is committed, so to enter the dev shell automatically on `cd`, just install [direnv](https://direnv.net) and allow it:

```sh
direnv allow
```

### Without Nix

Requires CMake ≥ 3.25, a C++17 compiler, `libunwind`, and [Node.js](https://nodejs.org) (runs the vendored FRET formaliser CLI). All other dependencies are fetched automatically by CMake.

```sh
cmake --workflow --preset debug      # configure + build + test
cmake --build build                  # incremental build only
```

## Usage

```
counter --input <spec.json> --output-dir <dir> [--config <file.toml>] [--seed <n>]
compare --repairs <dir> --ideal <file> [--ideal <file>...]
realize <spec.json> [<spec.json> ...]
ltl <spec.json> [<spec.json> ...]
```

Run any binary with `--help` for full option descriptions.

## How it works

Counter repairs an unrealisable FRETISH specification by running a genetic
algorithm over the space of FRETISH requirements:

1. **Seed** a population of `population_size` (default 200) specifications from
   the input, each mutated slightly from the original.
2. **Score** each candidate with a weighted fitness function combining:
   - *Semantic similarity* — bounded model counting of satisfying LTL traces
     (dominant component, weight 0.5).
   - *Realizability status* — `ltlsynt` outcome mapped to {0, 0.1, 0.2, 0.5,
     1.0} (weight 0.5).
   - *Syntactic similarity* — shared sub-formula count normalised to [0, 1]
     (weight 0.2).
   - *Halstead complexity penalty* — penalises candidates larger than the
     original (weight 0.1).
3. **Evolve** for `generations` (default 10) rounds: truncation selection →
   crossover → mutation → per-generation filters (false-trigger removal,
   deduplication, optional weakening).
4. **Collect** realisable survivors, apply the implication filter to keep only
   maximal repairs, and write each to `<output-dir>/repair_N.json`.

Model counting uses [Ganak](https://github.com/meelgroup/ganak) on the
transition matrices of SPOT-generated automata. Satisfiability and
realizability queries use [black](https://www.black-sat.org) and
[ltlsynt](https://spot.lre.epita.fr) respectively.

See [`docs/architecture.rst`](docs/architecture.rst) for a full description of
the key types and module layout.

### Configuration file

Algorithm parameters (population size, fitness weights, mutation rates, etc.)
can be tuned without recompiling by passing a TOML file to `--config`.  All
keys are optional — absent keys keep their built-in defaults:

```toml
[genetic]
generations     = 20   # double the default evolution rounds
population_size = 500

[runtime]
parallel = 16              # override thread pool size
report_cpu_timing = true   # print a CPU-attribution report at the end:
                           # your code vs. the external CLI tools (black,
                           # ltlsynt, ganak, ...), measured per-process via
                           # getrusage/wait4. Defaults to false.
```

A fully-annotated template with every key and its default is provided in
[`example-config.toml`](example-config.toml).

## Dependencies

When using the Nix dev shell (`nix develop`), all build-time dependencies are provided automatically. The following are fetched or built by CMake at configure time regardless of workflow:

| Dependency | How obtained |
|---|---|
| [Ganak](https://github.com/meelgroup/ganak) | Pre-built binary — [`cmake/ganak.cmake`](cmake/ganak.cmake) |
| [Spot](https://spot.lre.epita.fr) | Built from source — [`cmake/spot.cmake`](cmake/spot.cmake) |
| [Black](https://www.black-sat.org) | Pre-built `.deb` (Ubuntu 24.04 x86\_64) or built from source — [`cmake/black.cmake`](cmake/black.cmake) |
| Eigen, nlohmann\_json, tomlplusplus, cpptrace | FetchContent (header-only) — [`cmake/dependencies.cmake`](cmake/dependencies.cmake) |

Without Nix, you need CMake ≥ 3.25, a C++17 compiler (gcc ≥ 7 or clang ≥ 5), `libunwind`, and [Node.js](https://nodejs.org) installed system-wide.

> **Note on Node.js:** the FRET requirement-formaliser CLI is vendored as a plain script ([`vendor/fretCLI.main.js`](vendor/fretCLI.main.js), see [`vendor/README.md`](vendor/README.md)) and run with `node` looked up on `PATH` at run time (`runner/formaliser.hpp`) — it is not built or fetched by CMake, so `node` must be installed separately.

> **Note on `libfmt`:** `black` requires `libfmt.so.9` at runtime, whether it's a system binary found on `PATH` or the prebuilt `.deb` that `cmake/black.cmake` downloads as a fallback — the `.deb` does not bundle it. The Nix dev shell provides this via the `fmt_9` package; outside Nix, install `libfmt-dev` (or equivalent) system-wide.
