# Counter

Repairing unrealisable reactive specifications using a genetic algorithm. Inputs
are either *FRETISH* requirements (as JSON) or basic *TLSF* — the Temporal Logic
Synthesis Format used by the reactive-synthesis community.

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
counter --input <spec.json|spec.tlsf> --output-dir <dir> [--config <file.toml>] [--format <fretish|tlsf>] [--seed <n>]
compare --repairs <dir> --ideals <dir>
realize <spec.json|spec.tlsf> [...]
ltl <spec.json|spec.tlsf> [...]
```

All four binaries accept both input formats. The format is inferred from the
file extension — a `.tlsf` file is read as TLSF, anything else as FRETISH — or
forced with `counter --format`. Run any binary with `--help` for full option
descriptions.

## How it works

Counter repairs an unrealisable specification by running a genetic algorithm
over the space of candidate specifications. The description below is in terms of
FRETISH requirements; TLSF inputs run through the same four stages, with the
differences noted under [TLSF specifications](#tlsf-specifications):

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
   maximal repairs, and write each to `<output-dir>/` — as `repair_N.json` for
   FRETISH, or `repair_N.tlsf` for TLSF.

Model counting uses [Ganak](https://github.com/meelgroup/ganak) on the
transition matrices of SPOT-generated automata. Satisfiability and
realizability queries use [black](https://www.black-sat.org) and
[ltlsynt](https://spot.lre.epita.fr) respectively.

See [`docs/architecture.rst`](docs/architecture.rst) for a full description of
the key types and module layout.

### TLSF specifications

Alongside FRETISH, counter repairs basic-TLSF specifications directly. A `.tlsf`
input is auto-detected (or forced with `--format tlsf`), and the same genetic
machinery evolves its six sections — `INITIALLY`, `PRESET`, `REQUIRE`, `ASSUME`,
`ASSERT`, and `GUARANTEE` — rather than FRETISH requirements. Repairs are
rendered back to TLSF as `repair_N.tlsf`, each paired with a
`repair_N.fitness.json` score breakdown.

```sh
counter --input examples/arbiter-gr1/spec.tlsf --output-dir out
```

The bundled [`examples/arbiter-gr1`](examples/arbiter-gr1) is a two-client
mutual-exclusion arbiter that is unrealisable without request fairness. The
guarantees demand that each client is granted infinitely often (`G F g0`,
`G F g1`) while never granted simultaneously, but nothing forces the environment
to keep requesting — so the system cannot comply. Counter repairs it the way
[`ideal.tlsf`](examples/arbiter-gr1/ideal.tlsf) does: by strengthening the
environment with the fairness assumptions `G F r0` and `G F r1`. This
environment-strengthening move is the same `--config`-tunable `p_add_assumption`
mutation used in FRETISH mode.

The fitness function mirrors the FRETISH one — semantic similarity (bounded
model counting), realisability status, syntactic similarity, and a Halstead
size penalty — scored over whole TLSF formulae. Because a repair is written as
valid TLSF, it can be fed straight back into `realize`, `ltl`, or a synthesiser.

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
