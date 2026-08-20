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
| `coverage` | `build-coverage/` | clang only; source-based coverage instrumentation, see [below](#coverage) |

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

### Coverage

The `coverage` preset compiles with clang's *source-based coverage* instrumentation, `-fprofile-instr-generate -fcoverage-mapping`, as a Debug build under `build-coverage/`. It is clang only, since gcc rejects both flags and `--coverage` with gcov would report a different number from a different tool. The preset's test half points `LLVM_PROFILE_FILE` at `build-coverage/profraw/%p.profraw`, so running the suite any other way leaves no profile to measure.

One script is the whole workflow.

```sh
python scripts/coverage_badge.py           # build, test, write docs/coverage.svg
python scripts/coverage_badge.py --check   # fail if the committed badge is stale
```

It configures, builds, runs `ctest --preset coverage`, merges the raw profiles with `llvm-profdata`, exports a summary with `llvm-cov export --summary-only` over `src/` and `include/`, and writes `docs/coverage.svg`. Stale profiles are deleted first, because they accumulate and an old one credits lines this build may not even contain. A failing suite refuses to write the badge. `--json <file>` reuses an llvm-cov export already made, and `--no-run` re-exports the profiles already on disk.

The badge is a committed file rather than a call out to a badge service, so the README renders on a fork with no secrets and in an offline clone. That holds only while the file is regenerated when the number moves, which is what `--check` is for. It runs as the tail of the `coverage` entry in the build matrix of [`.github/workflows/ci.yml`](../.github/workflows/ci.yml), on every push and pull request, with `--no-run` so that it reads the profiles that entry's own `ctest` wrote rather than measuring the tree a second time.

The measurement covers every instrumented binary rather than the test binary alone: `counter`, `compare`, `lint-ideals`, `ltl`, `mucs`, `realize`, `signal_tracer` and `test/counter_tests`. Over `counter_tests` by itself the figure is 86.7%, and over all eight it is 71.0%. The difference is the driver code (`src/repair/*.cpp`, `src/compare.cpp`, `src/lint_ideals.cpp`) that no test runs, which is uncovered rather than absent and belongs in the denominator. A new binary goes into `BINARIES` in the script, which fails loudly when a name it holds is not built.

`llvm-profdata` and `llvm-cov` must come from the same LLVM release as the clang that built the tree. The profile format is versioned, so an older tool reports a current profile as malformed. The Nix dev shell carries `llvmPackages.llvm` for this; on a host with several LLVMs installed, `LLVM_COV` and `LLVM_PROFDATA` override the lookup.

Without help, the first coverage configure rebuilds Spot, black and Ganak into `build-coverage/third_party`, and Spot alone is a 30-minute build. Linking the debug tree's copies in beforehand avoids that, since `cmake/spot.cmake` skips the external project when its done-stamp is already there:

```sh
mkdir -p build-coverage
ln -s ../build/third_party build-coverage/third_party
```

The figure moves by about a tenth of a percentage point between runs of one binary, because the suite spawns real tools and branches on their timings and peak resident set: `src/runner/process.cpp`, `black.cpp` and `spot.cpp` account for all of the movement measured so far. The badge prints a whole number, so that jitter matters only near a rounding boundary, and `--check` carries `CHECK_SLACK` for it — a quarter of a point of tolerance beyond the committed number's rounding band, which passes a run that landed the other side of a boundary and still fails a real drop. Regenerating the badge and committing it is what a genuine move calls for.

## Documentation

```sh
cmake --build build --target docs   # Doxygen + Sphinx + KaTeX, all three required
```

This builds the curated public site from the `include/` headers.

Maths in doc comments is written as Doxygen's `\f$ ... \f$` (inline) and `\f[ ... \f]` (display), and rendered by KaTeX rather than Sphinx's default MathJax. The site fetches nothing at view time: `sphinxcontrib-katex` copies its own JS into `_static`, and `cmake/docs.cmake` stages `katex.min.css` and its fonts beside it out of the `katex` package, which `KATEX_DIST_DIR` locates. Set that variable by hand if the package sits somewhere the search does not reach.
