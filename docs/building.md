# Building

Counter builds with CMake presets. There are two workflows: a Nix dev shell that provides every dependency, and a manual one that needs a handful of system packages installed first.

## With Nix

Requires [Nix](https://nixos.org/download/) with flakes enabled.

```sh
nix develop                          # enter dev shell (first run fetches dependencies)
cmake --workflow --preset debug      # configure + build + test
```

A `.envrc` (`use flake`) is committed, so the dev shell can be entered automatically on `cd` by installing [direnv](https://direnv.net) and allowing it:

```sh
direnv allow
```

## Without Nix

The following must be installed system-wide:

| Requirement | Notes |
|---|---|
| CMake ≥ 3.25 | |
| [Ninja](https://ninja-build.org) | the presets' generator |
| C++17 compiler | gcc ≥ 7 or clang ≥ 5 |
| `libunwind` | |
| `libfmt` version 9 | see [below](#libfmt) |
| [Node.js](https://nodejs.org) | see [below](#nodejs) |

Everything else is fetched or built by CMake at configure time.

```sh
cmake --workflow --preset debug      # configure + build + test
```

## Presets

| Preset | Build directory | Notes |
|---|---|---|
| `debug` | `build/` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `release` | `build-release/` | no debug info |
| `relwithdebinfo` | `build-relwithdebinfo/` | release-like, with debug info |
| `tsan` | `build-tsan/` | ThreadSanitizer; disables address space layout randomisation (ASLR) for the tests |
| `bench` | `build-bench/` | release, plus the benchmarks in [`bench/`](../bench) registered as tests |

`cmake --workflow --preset <name>` configures, builds and tests in one step. After the first configure, `cmake --build build` rebuilds incrementally.

Always go through a preset. `cmake -B build` on its own configures with CMake's default generator — Unix Makefiles on Linux — which conflicts with the Ninja tree every preset produces. The conflict reaches the sub-builds that CMake uses to fetch dependencies too, so the failure can be reported against one of those rather than against the generator. To recover, delete `CMakeCache.txt` and `CMakeFiles/` from the affected build directory and configure again with the preset. `build/third_party` survives that, so Spot is not rebuilt.

## Fetched dependencies

These are obtained by CMake regardless of workflow, so they need no manual installation:

| Dependency | How obtained |
|---|---|
| [Ganak](https://github.com/meelgroup/ganak) | pre-built binary — [`cmake/ganak.cmake`](../cmake/ganak.cmake) |
| [Spot](https://spot.lre.epita.fr) | built from source — [`cmake/spot.cmake`](../cmake/spot.cmake) |
| [Black](https://www.black-sat.org) | pre-built `.deb` (Ubuntu 24.04 x86\_64) or built from source — [`cmake/black.cmake`](../cmake/black.cmake) |
| Eigen, nlohmann\_json, tomlplusplus, cpptrace | FetchContent, header-only — [`cmake/dependencies.cmake`](../cmake/dependencies.cmake) |

Building Spot from source dominates the first configure. Subsequent configures reuse it.

### Node.js

The FRET requirement-formaliser command-line interface (CLI) is vendored as a plain script ([`vendor/fretCLI.main.js`](../vendor/fretCLI.main.js), see [`vendor/README.md`](../vendor/README.md)) and run with `node` looked up on `PATH` at run time. CMake neither builds nor fetches it, so `node` must be installed separately. The Nix dev shell provides it via the `nodejs` package.

### libfmt

`black` needs `libfmt.so.9` at run time, whether it is a system binary found on `PATH` or the pre-built `.deb` that `cmake/black.cmake` downloads as a fallback — the `.deb` does not bundle it. The Nix dev shell provides this via the `fmt_9` package; otherwise install `libfmt-dev` (or the equivalent) system-wide.

## Tests, linting and formatting

```sh
ctest --preset debug                       # run all tests
ctest --preset debug -R syntactic          # run tests matching a regex

cmake --build build --target lint          # cpplint + clang-tidy + cppcheck + config key parity
cmake --build build --target format        # apply clang-format in-place
cmake --build build --target format-ci     # dry-run, fails if unformatted
```

## Documentation

```sh
cmake --build build --target docs   # Doxygen + Sphinx + KaTeX, all three required
```

This builds the curated public site from the `include/` headers.

Maths in doc comments is written as Doxygen's `\f$ ... \f$` (inline) and `\f[ ... \f]` (display), and rendered by KaTeX rather than Sphinx's default MathJax. The site fetches nothing at view time: `sphinxcontrib-katex` copies its own JS into `_static`, and `cmake/docs.cmake` stages `katex.min.css` and its fonts beside it out of the `katex` package, which `KATEX_DIST_DIR` locates. Set that variable by hand if the package sits somewhere the search does not reach.
