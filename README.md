# Counter

Repairing unrealisable FRETish specifications using a genetic algorithm.

## Build

```sh
cmake --workflow --preset debug   # configure + build + test
cmake --build build               # incremental build only
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
parallel = 16          # override thread pool size
```

A fully-annotated template with every key and its default is provided in
[`counter.toml.example`](counter.toml.example).

## Dependencies

| Dependency | Platforms |
|---|---|
| [Ganak](https://github.com/meelgroup/ganak) | Linux, macOS — pre-built binary via [`cmake/ganak.cmake`](cmake/ganak.cmake) |
| [Spot](https://spot.lre.epita.fr) | Linux, macOS — built from source via [`cmake/spot.cmake`](cmake/spot.cmake) |
| [Black](https://www.black-sat.org) | Linux x86\_64, macOS — see [`cmake/black.cmake`](cmake/black.cmake) |
