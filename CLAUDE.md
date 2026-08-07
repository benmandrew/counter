# counter

C++17 genetic algorithm for repairing unrealizable FRETISH specifications, using bounded model counting (SPOT + Ganak) for semantic similarity.

## Build

Enter the dev shell first (Nix is the primary workflow):

```sh
nix develop
```

Two presets — `debug` (ASAN+UBSAN, `build/`) and `release` (`build-release/`):

```sh
cmake --workflow --preset debug      # configure + build + test
cmake --workflow --preset release

# Incremental build only (after initial configure):
cmake --build build
cmake --build build-release
```

Non-Nix: requires CMake ≥ 3.25, Ninja (the presets' generator), a C++17 compiler, `libunwind`, and Node.js (runs the vendored FRET formaliser CLI, not fetched by CMake); everything else is fetched by CMake.

## Tests

```sh
ctest --preset debug                  # run all tests
ctest --preset debug -R syntactic     # run tests matching a regex
```

Test binaries land at `build/test/counter_tests`. The test framework uses `expect(bool, message)` and `fail(message)` from `test/test_support.hpp`; each test suite is a free function declared in `test/test_suite.hpp`.

The dashboard page's script is tested separately, under node's built-in runner (`test/web/*.test.mjs`, registered with ctest as `dashboard_page`; run directly with `node --test "test/web/*.test.mjs"`). `test/web/harness.mjs` extracts the `<script>` block from `web/dashboard.html` and evaluates it, so the tests run against the page that actually ships rather than a copy of it. CMake skips them when `node` is absent.

## Lint & Format

```sh
cmake --build build --target lint          # cpplint + clang-tidy + cppcheck + config parity
cmake --build build --target lint-cpplint
cmake --build build --target lint-clang-tidy
cmake --build build --target lint-cppcheck
cmake --build build --target lint-config-schema

cmake --build build --target format        # apply clang-format in-place
cmake --build build --target format-ci     # dry-run (fails if unformatted)
```

The same checks run from `.githooks/pre-commit`, which is tracked. Git refuses to clone hooks, so `core.hooksPath` has to be set locally once per clone; `cmake/githooks.cmake` does it at configure time, since the hooks shell out to build targets and cannot work before that anyway. Edit the hook in `.githooks/`, not in `.git/hooks/`, which git no longer reads once the path is set. Bypass a hook with `git commit --no-verify`; CI is the real enforcement.

## Config keys

Adding a TOML config key means editing three places, none of which the compiler ties together: the `apply_*` function in `src/config_io.cpp` that reads it, `config_key_spec()` in the same file (else the parser warns "unknown key" on a key it accepts), and `schemas/config-schema.json` (else editors reject it). `scripts/check_config_schema.py` enforces the last two against each other and against `example-config.toml`, and runs as part of `lint`.

`example-config.toml` is not merely illustrative: every value in it must equal the built-in default from `include/config.hpp`, so copying the file whole is a no-op. The same script enforces that, via a `DEFAULT_FIELDS` table mapping each TOML path to the `Config` member behind it — a new key must be added there (or to `UNPINNED_KEYS`, for the enums and for `runtime.parallel`, whose default is hardware concurrency) or the check fails rather than skipping it. This exists because eight keys had silently drifted, so the template quietly configured a different run from the one a user got by omitting it.

A config knob is worth keeping only if something turns it. Before adding one, and when auditing, count how many configs under `experiments/` actually set it — six per-filter run intervals, the `[syntactic]` component weights, `tlsf.mutation.p_guarantee` and `runtime.report_cpu_timing` were all removed on that evidence, having never been set in ~65k archived campaign configs.

## Commit provenance

Every binary answers `--version` with `commit=`, `commit_short=` and `dirty=` lines, naming the commit it was built from. The hash is resolved at **build** time, not configure time: `cmake/version.cmake` runs `cmake/write_version_header.cmake` in script mode on every build, which renders `cmake/version.hpp.in` into `build/generated/git_version.hpp` via `configure_file` (so the file is rewritten only when the commit changed). Resolving it at configure time instead would bake in whatever HEAD was when cmake last ran, and `cmake --build` after a new commit would keep reporting the old hash — the exact failure this exists to catch. Only `src/version.cpp` includes the generated header, so a new commit recompiles one translation unit. `dirty` counts modified *tracked* files only.

`scripts/run_experiments.py` shells `counter --version` once at startup — never `git rev-parse`, which reports the source rather than the binary — and stamps `commit` (abbreviated) and `dirty` onto every CSV row, plus a per-host `<stem>-manifest-<host>.json` beside the CSV. It refuses to launch when a binary's commit differs from the working tree's HEAD, was built dirty, or cannot be read at all; `--allow-stale-binary` downgrades that to a warning. Neither column may join the resume key in `run_experiments.py` or `KEY_FIELDS` in `merge_experiments.py`: archived rows have no commit, so keying on it would make every one of them miss and re-run finished campaigns.

Campaigns closed before this existed carry a reconstructed `PROVENANCE.json` in their archive directory (`"attribution": "inferred"`), covering the profile commit only; `binary_commit` is `null` there and must stay that way. `experiments/README.md` documents the method and why a commit landing inside a run window proves nothing.

Each archived campaign also carries `experiments/<campaign>/scripts/` — verbatim copies of the `gen_configs.py`, `run_experiments.py` and `merge_experiments.py` that ran it, so the directory reproduces without the git history. `vendored_scripts` in its `PROVENANCE.json` records the source commit and blob sha per file; check a copy with `git hash-object`. Attribution defaults to `profile_commit`, but campaign content overrides it where the two disagree: `CSV_FIELDS` in `run_experiments.py` grew one column at a time, so a header pins the runner revision independently of any mtime, and the profile's seed budget, spec set and timeout caps narrow it further. The column count alone is necessary but not sufficient — a runner must also define the profile the campaign ran. Four campaigns (`factorial`, `wellsep`, `genpop-sweeps`, `tlsf-genpop`) are attributed against their anchor on that basis; do not "correct" them back. The last two ran off the unmerged TLSF branch and their source commits are not ancestors of `main` (`reachable_from_main: false`); the annotated tag `provenance/tlsf-branch` points at that branch's tip and holds all of them reachable, so never delete it — without it they are dangling objects that `git gc` collects and the recorded shas stop resolving. `2026-07-31-replicate` sits in the same position for a different reason: its branch `feat/replicate-campaign` was split into reconstructed pull requests rather than merged, so the same content reached `main` under fresh shas while every sha its `PROVENANCE.json` names still points into the branch, and `provenance/replicate-campaign` holds those under the same never-delete rule. Splitting a branch instead of merging it silently invalidates any recorded sha pointing into it, so re-check a campaign archive whenever its branch is split rather than merged. These paths plus `README.md` and `experiments/<campaign>/PLAN.md`, a campaign's pre-registered plan where one was written before launch, are the only tracked files under `experiments/`, via negations in `.gitignore` that need each directory level to be ignored by content rather than by name; adding a tracked file under a new subdirectory means re-opening descent into it first. The plan is tracked so its decision rule stays checkable against the result it was meant to bind; the root `PLAN.md` stays gitignored, a working file with no such claim on it.

A campaign's archived config is a partial record: `gen_configs.py` emits a key only where a sweep overrides it, so everything else comes from the binary's default at run time. Changing a C++ default therefore changes what every archived config *means* without touching a byte of it. Two keys have crossed that line, both in the same commit: `allow_output_assumptions` and `run_well_separation` were `false` for every campaign archived under `experiments/` and are `true` from that commit on. Reproducing any archived campaign therefore requires writing both keys back to `false` explicitly. Archived configs are not edited to compensate; the note in `experiments/README.md` ("Config vintage") is the record. Flipping a default that archived configs omit means adding to that note.

*Removing* a key is the sharper version of the same problem: an archived config that sets one is no longer merely reinterpreted, it is rejected. The `[filters.intervals]` table went that way, which retired the `wellsep-timing` profile and TLSF sweep V along with it — that campaign's arms were three values of `well_separation`, so it cannot be regenerated at all and its archive is the only record. Retire the sweep and profile when their key goes; a generator that emits a key the binary warns on is worse than an absent one.

Retiring a key's *value* lands in the same place. The selection schemes were renamed on 2026-08-06 — `nsga2` to `nsga2-truncate`, `nsga2-replicate` to `nsga2-apportion` — and the old spellings are rejected rather than aliased, by name and with a message saying what to do. Every archived campaign config sets one of them, so none of them runs against a current binary; reproduce those at the commit their `PROVENANCE.json` names, which is what the vendored per-campaign `scripts/` exists for.

Archived *results* are the separate half, and the one that needed work. `gen_configs.py` turns the scheme name into a config directory name, `scheme_of()` in `run_experiments.py` reads it back into the `selection` column of every results CSV, and that column joins `KEY_FIELDS` in `merge_experiments.py` — so renaming what the harness emits would have made all 224,861 archived rows reading `nsga2` miss their resume key and re-run finished campaigns. `canonical_scheme()` in both scripts maps the retired spellings onto the new ones wherever a `selection` value is read back, which is what keeps resume and merge joining those rows. The scheme itself never changed, only its name, so the mapping is an identity on behaviour. Renaming any other factor directory means adding to that mapping in the same way; leaving it out is silent, and shows up as a finished campaign re-running from zero.

## Docs

Every header file in `include/` must have a corresponding `.rst` page under `docs/api/` and be listed in `docs/index.rst`. When adding a new header, add the page and toctree entry before committing. The site covers `include/` alone: implementation detail under `src/` is deliberately not published.

Only `///` reaches the site. A `//` comment on a declaration is invisible to Doxygen, which is why every `Config` member read as undocumented until its block was converted. The conversion is not free: `///` text goes through Doxygen's comment parser, so a bare `<input>` or `<output-dir>` parses as an HTML tag and `WARN_AS_ERROR` fails the build — wrap those in backticks, which also renders them as code.

## Code style

- **Standard**: C++17, `-Wall -Wextra -Wpedantic -Werror` on all targets.
- **Formatting**: clang-format (`.clang-format` at repo root). Run `format` target before committing.
- **Linting**: clang-tidy (`.clang-tidy`), cpplint (`CPPLINT.cfg`), cppcheck (`cppcheck_suppressions.txt`).
- **Comments**: only when the WHY is non-obvious — no narrating what the code does, no docstrings repeating parameter names already clear from the signature.
- **Assertions**: use `assert()` for internal invariants; `throw` only at API boundaries.
- **Overflow**: arithmetic on `Count` values must go through `count_add_overflow` / `count_mul_overflow` with an assert on the overflow flag.
- **Visitor pattern**: prefer `std::visit` with `if constexpr` branches over chains of `std::get_if` when dispatching on `std::variant` (see `requirement_to_ltl`, `mutate_timing`).

## Algorithm flow (`counter`)

1. Load `Specification` from `--input` JSON.
2. Build `AggregateWeightedFitnessFunction` (syntactic + semantic + Halstead + status) and per-generation `FilterFunction` list from the original spec.
3. Seed an RNG (from `--seed` or `std::random_device`); register crash metadata.
4. Run `Config::generations` rounds of `evolve_generation`: crossover + mutation, apply filters to the offspring (dedup, bloat cap, false-condition, optional vacuity/well-separation), then score the survivors in a thread pool. Filters run **before** scoring, so a dropped candidate never costs a model-count or a synthesis query, and a filter's solver calls warm the caches scoring then hits. A stage that re-tests a filter's own predicate on the population that filter already judged is therefore dead code, not a safety net. The one deliberate re-test is the filter fallback: when the chain empties the offspring, it re-applies the `FilterKind::Correctness` filters alone to the *unfiltered* offspring, a set those filters never saw, because dedup and the bloat cap run first and shadow them. It re-admits the candidates dropped for being duplicates or oversized and none of the ones dropped as unfit to breed from; with no correct offspring at all the elites carry the generation, and only an elite-free run restores them unscreened. Tagging a new filter is therefore load-bearing, and the untagged default is `Correctness` so a forgotten tag costs a wasted re-test rather than a re-admitted bad candidate. Selection follows `Config::selection_scheme`, whose two NSGA-II schemes rank identically and are named for the survivor step, the only thing that differs between them: `Nsga2Truncate` (TOML `nsga2-truncate`, the default) ranks by non-dominated sorting + crowding distance over the individual objectives (`include/genetic/nsga2.hpp`) with (μ+λ) survivor pooling, then truncates the pool at `population_size`; `Nsga2Apportion` (TOML `nsga2-apportion`) ranks identically but deduplicates the pool before sorting and apportions the `population_size` slots over the distinct survivors by `1 / (1 + rank)`; `WeightedAverage` ranks by the aggregate scalar, but converges prematurely and is kept for comparison rather than use. These were spelled `nsga2` and `nsga2-replicate` until 2026-08-06, when the old spellings were removed rather than aliased — see "Selection scheme rename" below, which is the only reason ~225k archived result rows still read `nsga2`. The scored population always carries both the per-objective vector and the weighted scalar (`Scored<Spec>`).
5. Collect the realizable survivors from the final population (re-checked with `black` + `ltlsynt`).
6. Apply final filters: dedup, then the optional weakening filter (keep only genuine weakenings of the original), then the optional implication filter to keep only maximal specs. Weakening is a final screen rather than a per-generation one because pruning non-weakenings mid-search measurably costs repair quality and never gains it; see `docs/configuration.rst`.
7. Score, sort, and write each maximal spec to `<output-dir>/repair_N.json`.

## Profiling

`COUNTER_PROFILE=<path>` turns on the *scope profiler* (`include/profile.hpp`), which writes a table to stderr and JSON to that path; `COUNTER_PROFILE=1` gives the table alone. Every binary reports. The report registers with `atexit` on the first scope opened, so `realize`, `mucs` and `compare` need no wiring of their own — and a binary that opens no scope prints nothing at all.

Read wall time against per-thread CPU time. A site with large wall and near-zero CPU is blocked on a child process, not computing: `proc/read` sits at a cpu/wall ratio of about 0.01 on a real run. That ratio is the diagnostic.

It is in-process instrumentation rather than `perf` or `gdb` because neither is available on the dev box — `kernel.perf_event_paranoid=4` and yama `ptrace_scope` rule out both. `strace` works only by launching the process, never by attaching. The counter registry is deliberately leaked so that it outlives the `atexit` report; destroying it first would free the names the report is about to print.

## Live dashboard

Opt-in, via `counter --dashboard` or `[runtime] dashboard = true` (the flag can only enable). Off by default so a campaign of many runs does not pay for the file and its flushes with nobody watching. When on, both drivers stream progress to `<output-dir>/progress.jsonl` (one JSON object per line, flushed as written) and copy `web/dashboard.html` there as `index.html`. To watch a run: `python3 -m http.server -d <output-dir> 8000`. The page polls once a second; `?poll=<seconds>` overrides that (`?poll=0` loads once and stops polling).

Each `stage` record carries `distinct` beside `n_in`/`n_out`: how many of the survivors are distinct specifications. The population is largely repeats, which no size can show, so this is the field that measures whether a selection scheme actually keeps diversity. Computing it hashes the whole population, so `run_generation_pipeline` only does so when an observer is attached — a run without the dashboard pays nothing.

The page's script keeps everything above its `boot()` call free of DOM access at load time: `boot()` runs only when `document` exists, and otherwise the script exports its functions for `test/web/` to test under node. Adding a top-level `document.getElementById` (rather than one inside a function) breaks that and takes the JS tests with it.

The page derives its stage list from the `stage` records of the latest generation, so a new filter or pipeline stage shows up with no change to either side. Generation stages come from `make_generation_pipeline` (`include/genetic/pipeline.hpp`), which returns an ordered vector of named `PipelineStage`s; `run_generation_pipeline` reports each to an optional `StageObserver`. Breeding must stay a single stage — crossover and mutation interleave per offspring slot, so splitting them reorders every RNG draw after the first and breaks seed reproducibility. The `determinism` test suite pins the draw stream against exactly that.

Two calls that both draw from the `RandomSource` must never be arguments of the same call: argument evaluation order is unspecified, and gcc and clang pick opposite orders, so a seed stops reproducing across compilers. Sequence each draw into its own local (as `rewrite_post_order` does). The CI matrix's gcc job is what catches this, since the goldens are pinned under clang.

## TLSF repair modes

Binaries: `counter` (genetic repair), `realize`, `compare`, `ltl`, `mucs` — run each with `--help` for flags.

`mucs` extracts a minimal unrealizable core. Prints the smallest subset of the guarantee-side sections (PRESET, ASSERT, GUARANTEE) that stays unrealizable against the full, unchanged environment side (INITIALLY, REQUIRE, ASSUME) — the culprit formulae behind unrealizability. Uses QuickXplain over `ltlsynt`. Prints `REALIZABLE (no core)` if the input is already realizable. TLSF-only (FRETISH JSON is not supported).

The same core extraction drives an alternative TLSF **repair mode**. `Config::repair_mode` (TOML `[tlsf] repair_mode = "monolithic" | "muc"`, default `monolithic`) selects between evolving the whole spec at once and the MUC-guided loop in `run_muc` (`src/tlsf/pipeline.cpp`): extract a core, evolve only that sub-spec, reintegrate the repaired core with the untouched non-core guarantees (`tlsf::reintegrate`), and repeat until the whole spec is realizable or `muc_max_iterations` trips. FRETISH ignores it. `scripts/gen_configs.py --repair both` and the `muc` profile in `run_experiments.py` cross the two modes as an experiment factor over the TLSF spec corpus.

## Tool subprocesses

Every pipe a runner opens must be created with `pipe2(..., O_CLOEXEC)` — `execute_and_capture` in `src/runner/process.cpp` and the formaliser's bidirectional pair in `src/runner/formaliser.cpp`. These runners are called from many scoring-pool threads at once, so a fork on one thread inherits the pipes every other in-flight call has open and holds them past its own exec. The reader waiting on such a call never sees end of file. `pipe2` sets the flag atomically; `pipe` followed by `fcntl` races a concurrent fork. The `dup2` onto the child's own stdin, stdout and stderr clears the flag on those copies, which is what lets the descriptors the child actually needs survive.

`spawn_piped_child` in `process.hpp` is the one fork behind any bidirectional child — currently the formaliser alone — so `ParentDeathPolicy` and `ExecutableLookup` stay the only two things a second user would differ in. Nothing may fork outside `process.cpp`: `posix_spawn` in particular has no attribute for `PR_SET_PDEATHSIG`, which is the whole reason these run on `fork` at all.

A tool's peak resident set cannot be measured below counter's own. A forked child starts as a copy-on-write copy of its parent, and `exec` folds that pre-exec high-water into the child's `maxrss`, so `wait4` reports `max(parent RSS at fork, the tool's true peak)`. `ProcessResult` therefore carries `m_peak_rss_floor_kb` — counter's resident set sampled just before the fork — and reports `m_peak_rss_kb` as zero at or below it rather than passing counter's own footprint off as the tool's. The `tool/<name>/rss_*` counters follow: `calls` counts every invocation, `rss_measured` only those that cleared the floor, and the max, total and threshold counters are over the latter alone, so a mean is `rss_kb_total / rss_measured`. Any change here has to keep that distinction; the `process_runner` suite pins it by spawning a bare shell while holding a 512MB buffer.

`simplify_ltl` runs one `ltlfilt` exec per cache miss. Coalescing concurrent misses into one exec was tried and removed: measured over the corpus at `parallel = 8`, the misses do not coincide, so the mean batch size was 1.012, exec count fell 1.9%, and wall time rose 37% on 46 of 46 paired examples. Any second attempt has to show a batch size above 1 before anything else about it matters.

## External tools

- `ltl2tgba`, `ltlsynt` — from SPOT, built from source via `cmake/spot.cmake`; located via the `SPOT_BIN_DIR` compile macro.
- `black` — LTL satisfiability checker (`black-sat`); found on `PATH` or downloaded/built via `cmake/black.cmake`; path passed as `BLACK_EXECUTABLE_PATH`. **Always run with a timeout**: `black -t <seconds> ...`.
- `ganak` — model counter; downloaded as a release binary via `cmake/ganak.cmake`; path passed as `GANAK_EXECUTABLE_PATH`.
- `node` — runs the vendored FRET formaliser CLI (`vendor/fretCLI.main.js formalize --logic ft-inf --batch`, see `runner/formaliser.hpp`); looked up on `PATH` at run time, not built or fetched by CMake, so it must be installed separately (`nodejs` in `flake.nix`).

## Key types

- `Timing` — `std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks, AfterTicks, Eventually, Always>` (see `requirement.hpp`).
- `ConditionType` — `enum class { Trigger, Continual }` controlling how the condition activates a `Requirement`: rising-edge (Trigger) or at every timepoint where it holds (Continual).
- `Requirement` — holds `m_condition`, `m_response`, `m_timing`, `m_condition_type`, the derived `m_ltl` string, and `m_weakenable`. When `m_weakenable` is false the requirement is locked: the genetic algorithm never mutates it, uses it as a crossover source, or simplifies it. It is part of `Requirement`'s identity (`operator<`/`==`/`hash`). Serialised as the optional JSON key `weakenable` (default `true`, emitted only when `false`).
- `Formula` — propositional AST with `syntactic_similarity`, `rewrite_post_order`, `n_subformulae`.
- `TransferSystem` — weighted automaton transition matrix for bounded model counting.
- `Count` — `long double` (x87 80-bit: integers exact to 2^64, exponent range to 2^16384). Trace counts are only consumed as ratios cast to `double`, so range matters more than exact integer width; `count_add_overflow`/`count_mul_overflow` report overflow as a non-finite result.
