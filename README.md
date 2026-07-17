# Counter

Counter repairs unrealisable reactive specifications using a genetic algorithm.

A reactive specification is *unrealisable* when no implementation can satisfy it
against every environment — the specification is at fault, not the code. Tools
like `ltlsynt` will say so, but not what to change. Counter searches for the
repairs: edits to the specification that make it realisable while staying as
close as possible to what was originally written.

Inputs are either *FRETISH* requirements as JSON, or basic *TLSF* — the Temporal
Logic Synthesis Format used by the reactive-synthesis community.

## Quickstart

Build, then repair the bundled arbiter example ([building from
source](docs/building.md) covers the non-Nix route):

```sh
nix develop
cmake --workflow --preset release

./build-release/realize examples/arbiter-gr1/spec.tlsf
# examples/arbiter-gr1/spec.tlsf: UNREALIZABLE

mkdir -p out
./build-release/counter --input examples/arbiter-gr1/spec.tlsf --output-dir out --seed 42
```

That writes 19 candidate repairs to `out/`, best first, each `repair_N.tlsf`
paired with a `repair_N.fitness.json` holding its score. Expect roughly 10–30 s
on 20 threads; the seed fixes the repairs, not the runtime, which swings with how
the external solvers get scheduled.

The example is a two-client arbiter that must grant each client infinitely
often, but nothing forces clients to keep requesting — so it cannot be
implemented. The top repair adds the missing fairness assumptions, `G F r0` and
`G F r1`, recovering the standard fix. Lower-ranked repairs instead weaken the
guarantees, which also works but is rarely what you want; reading down the
fitness order is the intended way to use the output. The [TLSF
guide](https://benmandrew.com/docs/counter/tlsf.html) walks through this run in
full.

## Commands

```
counter --input <spec.json|spec.tlsf> --output-dir <dir> [--config <file.toml>] [--format <fretish|tlsf>] [--seed <n>]
compare --repairs <dir> --ideals <dir>
realize <spec.json|spec.tlsf> [...]
ltl     <spec.json|spec.tlsf> [...]
mucs    <spec.tlsf>
```

| Command | Purpose |
|---|---|
| `counter` | repair an unrealisable specification |
| `realize` | report whether a specification is realisable |
| `ltl` | print the LTL formulae a specification translates to |
| `compare` | compare a directory of repairs against known-ideal ones |
| `mucs` | extract a minimal unrealizable core from a TLSF specification |

`counter`, `realize`, `ltl`, and `compare` accept both input formats. The format
is inferred from the file extension — `.tlsf` is read as TLSF, anything else as
FRETISH — or forced with `counter --format`. `mucs` is TLSF-only. Run any
command with `--help` for full option descriptions.

`mucs` isolates *why* a specification is unrealizable: it returns the smallest
subset of the guarantee-side sections (PRESET, ASSERT, GUARANTEE) that stays
unrealizable against the full, unchanged environment side, via QuickXplain over
`ltlsynt`. On the unrealizable arbiter, for instance, it pares five guarantees
down to the two — `G(g0 -> r0)` and `G F g0` — that actually conflict, pointing
repair at the culprits instead of the whole coupled spec.

## How it works

Counter evolves a population of candidate specifications over several
generations, keeping those that are realisable and close to the original.

1. **Seed** a population of `population_size` (default 200) specifications, each
   mutated slightly from the input.
2. **Score** each candidate on four weighted components: semantic similarity
   (bounded model counting of satisfying traces), realisability status,
   syntactic similarity, and a Halstead size penalty.
3. **Evolve** for `generations` (default 10) rounds of selection, crossover,
   mutation, and filtering.
4. **Collect** the realisable survivors, keep only the maximal ones under
   implication, and write each to the output directory.

Model counting uses [Ganak](https://github.com/meelgroup/ganak) over the
transition matrices of SPOT-generated automata. Satisfiability and realisability
queries use [black](https://www.black-sat.org) and
[ltlsynt](https://spot.lre.epita.fr) respectively.

## Documentation

The full documentation is published at
[benmandrew.com/docs/counter](https://benmandrew.com/docs/counter/).

| | |
|---|---|
| [Building from source](docs/building.md) | Nix and non-Nix builds, dependencies, presets, tests |
| [Architecture](https://benmandrew.com/docs/counter/architecture.html) | algorithm flow, key types, module layout |
| [Configuration](https://benmandrew.com/docs/counter/configuration.html) | tuning via TOML, fitness weights, selection schemes |
| [TLSF specifications](https://benmandrew.com/docs/counter/tlsf.html) | TLSF mode and a worked repair |
| [API reference](https://benmandrew.com/docs/counter/) | the `include/` headers, plus an internal reference covering `src/` |
| [Experiment scripts](scripts/README.md) | parameter sweeps and result analysis |

## Licence

See [LICENCE](LICENCE).
