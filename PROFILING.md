# Profiling `counter`

A record of one profiling pass: the harness built to take the measurements, the numbers it
returned, the one change applied so far, and the targets left ranked by size.

## The harness

`include/profile.hpp` and `src/profile.cpp` add a named-scope profiler. It is always compiled in
and inert unless the `COUNTER_PROFILE` environment variable is set. Set it to a file path to get a
JSON report as well as the table; set it to `1` or `-` for the stderr table only.

`COUNTER_PROFILE_SCOPE("name")` is a *resource acquisition is initialisation* (RAII) timer. Each
site records four things: call count, total wall time, total per-thread central processing unit
(CPU) time from `CLOCK_THREAD_CPUTIME_ID`, and the slowest single call.

Recording wall and CPU separately is the whole point. A scope whose CPU time is near zero but
whose wall time is large is *blocked*, not working. That one ratio separates waiting on a child
process from burning CPU, and the pre-existing per-tool timers could not draw that line at all.
Sites are function-local statics with atomic accumulators, so concurrent scoring workers do not
serialise on the profiler itself.

Instrumented so far:

- the subprocess path in each runner, split into `proc/fork+exec`, `proc/read` and `proc/wait`;
- `ltlfilt/simplify_ltl` and its cache lookup;
- `dispatch/collect-one-ready` in `include/bounded_async.hpp`.

This complements the existing "Tool timing report" in `src/main.cpp` rather than replacing it. That
report says how long each external tool took; it does not say where inside a call the time went.

`perf record` is unavailable on this machine (`kernel.perf_event_paranoid=4`), which is why the
pass used in-process instrumentation instead of sampling.

## The measurement

Workload: `counter --input examples/fsm/spec.json --seed 0`, 20 generations, population 1000,
NSGA-II selection, RelWithDebInfo build, 20 cores.

Baseline whole-run figures: 6.75 s wall, 19.06 s user CPU, 40.94 s system CPU, 14,147,580 minor
page faults.

| site | calls | wall | cpu | cpu/wall |
|---|---|---|---|---|
| `proc/read` | 4014 | 58.711 s | 0.282 s | 0.00 |
| `ltlfilt/simplify_ltl` | 106807 | 28.859 s | 1.807 s | 0.06 |
| `proc/fork+exec` | 4014 | 5.257 s | 2.345 s | 0.45 |
| `dispatch/collect-one-ready` | 11535 | 4.396 s | 0.372 s | 0.08 |
| `ltlfilt/simplify_ltl:cache-lookup` | 106807 | 0.494 s | 0.079 s | 0.16 |
| `proc/wait` | 4014 | 0.053 s | 0.049 s | 0.92 |

The wall times exceed the 6.75 s run because scopes on 20 worker threads are summed.

Tool calls for the same run: `ltlfilt` 2165 calls plus 104,642 cache hits, `black` 818 plus 65,537,
`ltlsynt` 565 plus 10,556, `ltl2tgba` 466 plus 28, `ganak` 34 plus 337.

## The change applied

`fork()` plus `execv()` became `posix_spawn()` in `src/runner/ltlfilt.cpp` and
`src/runner/black.cpp`. glibc implements `posix_spawn` with `clone(CLONE_VM|CLONE_VFORK)`, which
copies no page tables and does not write-protect the parent's address space.

`proc/fork+exec` parent CPU fell from 2.345 s to 0.987 s and its wall from 5.257 s to 3.373 s. The
whole run went from 6.75 s to 6.38 s, user CPU from 19.06 s to 17.97 s, system CPU from 40.94 s to
38.49 s. All 12 output `repair_N.json` files were byte-identical before and after, so the change
preserves behaviour.

`src/runner/spot.cpp`, which spawns `ltlsynt` and `ltl2tgba`, deliberately keeps `fork()`. Its
child calls `prctl(PR_SET_PDEATHSIG, SIGKILL)` so a killed run cannot leave multi-gigabyte orphan
processes behind, and `posix_spawn` has no attribute that does the same. The exception is
intentional and should stay.

## The real finding

The first hypothesis was wrong. It read the 40.94 s of system time as copy-on-write faults taken
in the parent because of `fork()`. The `posix_spawn` change refutes that: minor faults fell only
from 14,147,580 to 13,761,454, about 3%.

Direct measurement locates the faults instead:

- one trivial `ltlfilt --simplify -f 'a'` costs 2677 minor faults; one trivial
  `ltl2tgba -D -S -H -f 'a'` costs 3269. Multiplied by roughly 4000 spawns, that is essentially the
  entire 13.8 M count;
- 200 sequential `ltlfilt` spawns on the formula `a & b`: 1.80 s wall, 0.47 s user, 1.32 s system,
  551,349 faults;
- the same 200 formulae through *one* `ltlfilt` process via `-F <file>`: 0.00 s wall, 0.00 s user,
  0.00 s system, 2701 faults.

The cost is each child demand-paging its own executable and `libspot` on every `exec`. It is a
fixed per-spawn tax of roughly 9 ms wall and about 2750 faults, and it does not depend on how hard
the formula is. Across the run's roughly 4000 spawns that is on the order of 36 s of startup
against roughly 66 s of total external-tool wall time. More than half of tool time is startup.

## Ranked remaining targets

**Persistent or batched tool processes.** The largest lever by a wide margin. `ltlfilt` already
accepts batch input on `-F -`, and the codebase already holds the pattern: `PersistentProcess` in
`src/runner/formaliser.cpp` keeps one long-lived `node` child and talks to it over pipes. Applying
it to `ltlfilt` (2165 spawns) and `black` (818 spawns) would remove most of the startup tax. It
needs a request/response protocol, per-request timeout handling, and serialisation of the shared
pipe across scoring threads.

**Link libspot in process.** SPOT is already built from source, and `libspot.so` and its headers
sit in the build tree. This removes the `ltlfilt`, `ltl2tgba` and `ltlsynt` spawns outright rather
than amortising them. It is a much larger change, and `ltlsynt`'s timeout and memory-cap behaviour
would have to be rebuilt in-process.

**`dispatch/collect-one-ready`.** 4.396 s of wall time for 0.372 s of CPU across 11,535 calls.
`run_bounded_async` polls each outstanding future with `wait_for(0ms)` in a loop, then sleeps 1 ms.
A completion queue signalled by the workers would cut both the dispatch latency and the poll's CPU.

**Five duplicated `execute_and_capture` implementations.** `src/runner/spot.cpp`,
`src/runner/black.cpp`, `src/runner/ltlfilt.cpp`, `src/runner/ganak.cpp` and
`src/runner/formaliser.cpp` each carry a near-identical copy. This is not a performance problem in
itself, but every fix to the spawn path has to be made five times, as this pass did.

## Reproducing

```sh
cmake --workflow --preset relwithdebinfo

COUNTER_PROFILE=/tmp/counter-profile.json \
  ./build-relwithdebinfo/counter --input examples/fsm/spec.json --seed 0
```

The scope table goes to stderr; the JSON goes to the path named in `COUNTER_PROFILE`. Setting the
variable to `1` or `-` gives the table alone.

The instructive part of this pass was how far a plausible explanation survived without being
checked: `fork()` and copy-on-write faults fit the baseline numbers well enough that fixing them
felt like the obvious next step, and the fix returned 3%. Measuring a single trivial spawn in
isolation, which took one command, said more than the whole-run profile did.
