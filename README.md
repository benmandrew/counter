# Counter

Repairing unrealisable FRETish specifications using a genetic algorithm.

## Build

```sh
cmake --workflow --preset debug   # configure + build + test
cmake --build build               # incremental build only
```

## Usage

```
counter --input <spec.json> --output-dir <dir> [--seed <n>]
compare --repairs <dir> --ideal <file> [--ideal <file>...]
realize <spec.json> [<spec.json> ...]
ltl <spec.json> [<spec.json> ...]
```

Run any binary with `--help` for full option descriptions.

## Dependencies

| Dependency | Platforms |
|---|---|
| [Ganak](https://github.com/meelgroup/ganak) | Linux, macOS — pre-built binary via [`cmake/ganak.cmake`](cmake/ganak.cmake) |
| [Spot](https://spot.lre.epita.fr) | Linux, macOS — built from source via [`cmake/spot.cmake`](cmake/spot.cmake) |
| [Black](https://www.black-sat.org) | Linux x86\_64, macOS — see [`cmake/black.cmake`](cmake/black.cmake) |
