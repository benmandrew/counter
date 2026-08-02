# Profiling `counter`

A record of one profiling pass: the harness built to take the measurements, the numbers it
returned, the changes applied so far, one bug found on the way, one design that was tried and
reverted, and the targets left ranked by size.

## What is verified

Measured, reproducible, and load-bearing for everything below:

- the fixed per-spawn startup tax and its size — roughly 9 ms wall and about 2750 minor page faults
  per external tool spawn;
- the *close-on-exec* (`O_CLOEXEC`) pipe-inheritance deadlock, reproduced directly and now fixed;
- `ltlfilt`'s output flushing behaviour, taken from an `strace` and from interactive driving;
- the parent-side central processing unit (CPU) cost difference between `fork()` and
  `posix_spawn()`, measured inside the spawn scope;
- the split of cost across the four fitness objectives, and the fact that the pure-CPU inner
  algorithms (`count_traces`, `guard_models`, `syntactic`, `halstead`) are cheap;
- that batching `ltlfilt` calls trades about 5% of wall time for about 19% of total CPU, with
  byte-identical output;
- byte-identical output across all 12 `repair_N.json` files before and after every change.

Not established: any whole-run wall-clock improvement from this pass. There is none. The batcher
makes wall time worse by a measured amount, and nothing else here moves it outside run-to-run
spread. Separating a real effect from that spread needs an *A/B test* whose two binaries run
alternately in one session; two sets of runs taken at different times cannot do it.

## The harness

`include/profile.hpp` and `src/profile.cpp` add a named-scope profiler. It is always compiled in
and inert unless the `COUNTER_PROFILE` environment variable is set. Set it to a file path to get a
JSON report as well as the table; set it to `1` or `-` for the stderr table only.

`COUNTER_PROFILE_SCOPE("name")` is a *resource acquisition is initialisation* (RAII) timer. Each
site records four things: call count, total wall time, total per-thread CPU time from
`CLOCK_THREAD_CPUTIME_ID`, and the slowest single call.

Recording wall and CPU separately is the whole point. A scope whose CPU time is near zero but
whose wall time is large is *blocked*, not working. That one ratio separates waiting on a child
process from burning CPU, and the pre-existing per-tool timers could not draw that line at all.
Sites are function-local statics with atomic accumulators, so concurrent scoring workers do not
serialise on the profiler itself.

Instrumented so far:

- the subprocess path in each runner, split into `proc/fork+exec`, `proc/read` and `proc/wait`;
- `ltlfilt/simplify_ltl` and its cache lookup;
- `dispatch/collect-one-ready` in `include/bounded_async.hpp`;
- one scope per fitness objective, so each objective's cost is separated from the others;
- `count/count_traces` (`src/fitness/model_counter.cpp`), `count/build_transfer_from_hoa` and
  `count/guard_models` (`src/fitness/transfer_matrix.cpp`), and
  `fitness/syntactic_similarity_spec` (`src/fitness/syntactic_similarity.cpp`).

The per-objective scopes need a second entry point. Objective names are known only at run time, so
`profile::site_interned` *interns* a run-time name — it maps the string to a stable site once and
returns a handle to it. The aggregate fitness function resolves its sites when it is constructed,
not on every call, so an objective is never charged for the profiler's string handling.

The profiler therefore covers the in-process side as well as the subprocesses. This complements the existing "Tool timing report" in `src/main.cpp` rather than replacing it. That
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

The in-process numbers move and are stable, because they are measured inside the spawn scope rather
than across a whole run: `proc/fork+exec` parent CPU fell from 2.345 s to 0.987 s, and its wall from
5.257 s to 3.373 s. All 12 output `repair_N.json` files stayed byte-identical, so behaviour is
unchanged.

The whole-run effect is a different matter, and it is not established. An earlier single sample
read 6.75 s to 6.38 s, but three repeat runs of the same binary contradict it: 6.92 s wall / 19.86 s
user / 42.68 s system / 14,025,724 faults; 6.76 s / 19.47 s / 42.01 s / 13,919,322 faults; and
6.89 s / 18.86 s / 41.13 s / 13,941,620 faults. The spread across identical runs is wider than the
claimed gain. The change is kept on the narrow grounds that it costs less parent CPU per spawn and
changes no output — not as a headline speedup.

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

## The `O_CLOEXEC` bug

A latent bug surfaced during this work and is now fixed. A subprocess spawned while another tool
call was in flight inherited that call's pipe *file descriptors*, which held the write end of a
stranger's pipe open. The waiting reader then never saw end of file (EOF), because a pipe reports
EOF only once every holder of the write end has closed it. The child that reader was actually
waiting on had already exited and become a *zombie*.

The state was observed directly: the main thread asleep in `run_bounded_async`'s poll, one defunct
`ltl2tgba`, and eight live `ltlfilt` children holding the descriptor open.

One-shot children masked this for years. They exec and exit in milliseconds, so the window in which
one of them holds a stranger's pipe is too small to matter. A long-lived child never closes it at
all, which is why introducing persistent workers turned a rare race into a reliable hang.

The formaliser's persistent `node` child (`PersistentProcess`, `src/runner/formaliser.cpp`) has
exactly this shape and has been inheriting one-shot pipes all along, so the fix is not only for new
code. That makes it a plausible contributor to the long-run hangs and orphaned multi-gigabyte
`ltl2tgba` processes seen previously. Nothing here confirms that link; it is a hypothesis and
should be treated as one.

The fix: every runner pipe is now created with `pipe2(..., O_CLOEXEC)`. The `dup2` file actions that
hand a child its own stdin and stdout clear the flag on those descriptors, so intended redirections
are unaffected.

## Why a persistent `ltlfilt` does not work

A persistent-worker pool was built against `ltlfilt` and then reverted. The reason is a property of
the tool, so it is worth recording rather than rediscovering.

`ltlfilt` does not answer one line per line. An `strace` of a run shows it reply to the first
formula and then hold every later answer until its stdin reaches EOF:

```
read(0, "true\n")           = 5
write(1, "1\n")             = 2
read(0, "((iap_state_no…")  = 58
read(0, "")                 = 0        <- blocks here until exit
write(1, "iap_STATE_FAULT | …") = 51
```

Driving it interactively confirms the shape. Sending four formulae and reading after each gives
`'1'`, TIMEOUT, `'a'`, TIMEOUT. Replies arrive in irregular lumps whose timing depends on
accumulated output size, not on request boundaries. `stdbuf -oL` and `stdbuf -o0` change nothing,
because SPOT writes through C++ streams detached from stdio, so an `LD_PRELOAD` that adjusts stdio
buffering never sees the writes.

In the real program the consequence was unambiguous. Every pool worker's first request hit the 10 s
response deadline and retired that worker, after which every later call fell back to a one-shot
`exec`. Whole-run wall time went from 6.75 s to 25.93 s. The fallback-reason counters showed exactly
8 poll timeouts, one per pool worker, and 79 subsequent requests served by retired workers.

The payoff measured earlier is real, but it is available only in the *batch* shape: write every
formula, close stdin, then read every reply. That is what the 200-formula measurement did. Getting
it needs a batch entry point rather than a drop-in replacement for `simplify_ltl`, which is called
deep inside fitness evaluation where no batch of formulae exists to submit.

## The batched simplifier

That batch shape is now built and measured. `simplify_ltl` cache misses arriving from concurrent
scoring threads are coalesced into a single `ltlfilt` exec. The design follows from the constraint
above, point by point.

- A caller that misses the cache queues its formula and waits. Whichever caller finds no *leader*
  becomes the leader, takes everything queued at that instant, and runs one
  `ltlfilt --simplify --skip-errors -F -` over the whole batch: write every formula, close stdin,
  read every reply. Closing stdin is what makes `ltlfilt` emit everything, per the negative result
  above.
- There is no timer and no artificial delay. The leader takes whatever is queued right now. While
  its exec is in flight the other scoring threads pile up behind it, so the next batch is naturally
  as large as the contention warrants, and it collapses to a batch of one when only one thread is
  asking.
- Four independent batchers rather than one. A single leader would serialise every simplification in
  the process behind one exec at a time. Four run their execs concurrently, so throughput stays
  parallel while each exec still amortises its startup across a batch.
- The batch is capped at 64 formulae and 16 KB of input, so the whole batch fits in a pipe buffer.
  Without that cap the parent could block writing while the child blocks writing answers into an
  output pipe nobody is draining yet.
- The safety property is a line count. `--skip-errors` makes `ltlfilt` echo an unparseable line back
  verbatim instead of dropping it, so the reply count must equal the request count. If it does not,
  every formula in that batch falls back to its own exec. A silently misattributed simplification
  would corrupt the search rather than slow it down, which is why the check is on the count rather
  than on the content.
- Two inputs bypass batching: a formula containing a newline, which would be read as several
  requests, and a blank one, which `ltlfilt` consumes without answering and which would therefore
  fail the count check. The blank case is reachable — it is the specification formula of a candidate
  with no guarantees.

Results on `fsm` at 20 generations and population 1000, seed 0. The two binaries ran alternately in
one session, five repetitions each; the figures are medians.

| | baseline | batched | change |
|---|---|---|---|
| wall | 6.18 s | 6.50 s | +5.2% |
| user CPU | 19.80 s | 17.99 s | −9.1% |
| system CPU | 41.29 s | 31.32 s | −24.1% |
| minor page faults | 14.03 M | 11.80 M | −15.9% |

The two wall-time distributions do not overlap. Baseline: 6.09, 6.22, 6.18, 6.26, 6.15 s. Batched:
6.50, 6.51, 6.66, 6.41, 6.32 s. Every batched run is slower than every baseline run.

An earlier version of this section claimed wall time was unchanged. That claim was wrong. It rested
on three runs of one binary and three of the other, taken at different times on a machine whose load
varied, so the two sets were never comparable. Interleaving the runs removes that.

Batching therefore trades about 5% of wall time for about 19% of total CPU. The wall cost is
inherent to coalescing, not an implementation defect: a caller now waits for a leader to finish a
batch instead of running its own exec immediately. That wait is the mechanism that produces the CPU
saving, so no amount of tuning removes one and keeps the other.

Total spawns across the run fell from 4045 to 3262, and 2206 simplification requests were served by
1424 execs. All 12 `repair_N.json` outputs stayed byte-identical on every run.

The direction holds on other specs. These are one run of each build per spec rather than a careful
A/B, so they are indicative and nothing more:

| spec | wall, base → batched | system CPU, base → batched | faults, base → batched |
|---|---|---|---|
| `fsm` | 6.12 → 6.70 s | 41.70 → 33.05 s | 14.10 → 11.84 M |
| `takeoff` | 4.61 → 5.08 s | 25.32 → 23.91 s | 9.29 → 8.50 M |
| `fsm-timing` | 8.61 → 9.22 s | 56.53 → 43.86 s | 20.13 → 17.44 M |

Outputs were identical between the two builds on all three specs.

The number of batchers moves both figures along one curve, so the sweep behind the setting is
recorded here. Baseline and four batchers come from the five-repetition A/B; eight and 16 come from
three repetitions each. All figures are medians.

| batchers | wall | user CPU | system CPU | minor faults |
|---|---|---|---|---|
| baseline | 6.18 s | 19.80 s | 41.29 s | 14.03 M |
| 16 | 6.17 s | 19.48 s | 39.05 s | 13.83 M |
| 8 | 6.34 s | 19.12 s | 35.45 s | 13.22 M |
| 4 | 6.50 s | 17.99 s | 31.32 s | 11.80 M |

More batchers mean less waiting behind a leader and smaller batches, so wall time approaches the
baseline while the CPU saving shrinks. Fewer batchers mean the opposite. The relation is monotonic
and there is no setting that gets both: 16 batchers reach wall parity but save 4.2% of total CPU,
four save 19.3% of total CPU but cost 5.2% of wall.

Four stays the default, and the campaign regime it is chosen for was measured rather than assumed.
Running four `counter` processes at once on the same 20-core machine — which is what a campaign
does, and what makes the machine CPU-saturated rather than latency-bound — and timing the whole
batch, three repetitions each:

| repetition | baseline | batched |
|---|---|---|
| 1 | 13.97 s | 12.36 s |
| 2 | 14.16 s | 12.49 s |
| 3 | 14.14 s | 12.25 s |

Median 14.14 s against 12.36 s: the batched build finishes the batch **12.6% sooner**, with no
overlap between the two sets. The sign of the effect reverses with concurrency. A single run pays
5% more wall for the CPU it gives back; four concurrent runs get that CPU back as throughput,
because the cores the baseline spent demand-paging `ltlfilt` are the same cores its neighbours
needed. This is the case the default is for. A single interactive run is better served by 16
batchers, or by the baseline.

One reporting artefact comes with this. The "Tool timing report" `ltlfilt` row now measures each
caller's wait, which includes time spent queued behind a leader, so its per-call figure is no longer
the cost of an `ltlfilt` exec.

Validation ran 5 examples across the FRETISH path and 3 across the TLSF path, two seeds each, with
no hang and with the batch fallback never firing once.

## Where the in-process time goes

The per-objective scopes answer a separate question: of the work `counter` does itself, what costs
anything? Same workload as above — `fsm`, 20 generations, population 1000, seed 0. The run scored
617 individuals.

| site | calls | wall | cpu | wall/call |
|---|---|---|---|---|
| `fitness/status` | 617 | 17.741 s | 0.741 s | 28.75 ms |
| `fitness/semantic` | 617 | 8.741 s | 0.621 s | 14.17 ms |
| `count/build_transfer_from_hoa` | 511 | 0.985 s | 0.098 s | 1.93 ms |
| `count/guard_models` | 511 | 0.930 s | 0.088 s | 1.82 ms |
| `fitness/syntactic` | 617 | 0.448 s | 0.128 s | 0.73 ms |
| `fitness/syntactic_similarity_spec` | 629 | 0.442 s | 0.125 s | 0.70 ms |
| `fitness/halstead` | 617 | 0.250 s | 0.073 s | 0.41 ms |
| `count/count_traces` | 511 | 0.001 s | 0.001 s | 0.003 ms |

Whole-run figures for this measurement: 6.08 s wall, 19.17 s user CPU, 38.61 s system CPU. Output
stayed byte-identical to the baseline's 12 `repair_N.json` files.

`status` and `semantic` are 26.5 s of roughly 27.2 s of total objective wall time. Both are
dominated by external tools — `ltlsynt` and `black` for `status`, `ltl2tgba` and Ganak for
`semantic`. Both have a cpu/wall ratio near zero, so the scoring thread is blocked on a child
process rather than computing.

The two objectives that are pure in-process computation are `syntactic` and `halstead`. Together
they account for under 0.7 s of wall time and about 0.2 s of CPU across the whole run. Optimising
either would buy nothing.

The algorithms that a static reading of the code flags as risky are not hot in practice.
`count_traces` performs the repeated-squaring matrix power, which is cubic in state count, and costs
1 ms across the entire run. `count/guard_models` is exponential in the number of atoms a transition
guard does not mention, and costs 0.088 s of CPU. Measurement contradicts complexity analysis here,
and the measurement is the one to trust: on current evidence neither should be touched.

The wider result is that `counter`'s own computation is not the bottleneck. Nearly all of the run is
spent waiting on external tools, and more than half of that wait is per-process startup rather than
solving. This reinforces the top two ranked targets below instead of adding a third.

## Ranked targets

**Batched tool calls — done for `ltlfilt`.** The largest lever on CPU, and the lever is eliminating
per-spawn startup rather than keeping a process warm. It buys about 19% of total CPU and costs about
5% of wall time; the design and the numbers are in "The batched simplifier" above. Two parts of this
target are untouched. `black` accounts for 818
execs on this workload and has no batch mode of its own, so nothing here applies to it. `ltl2tgba`
and `ltlsynt` are still one exec per call.

**Link libspot in process.** SPOT is already built from source, and `libspot.so` and its headers
sit in the build tree. This removes the `ltlfilt`, `ltl2tgba` and `ltlsynt` spawns outright rather
than amortising them, and it sidesteps the flushing problem entirely by removing the process
boundary — which now counts in its favour. It is a much larger change, and `ltlsynt`'s timeout and
memory-cap behaviour would have to be rebuilt in-process.

**`dispatch/collect-one-ready` — done.** `run_bounded_async` polled each outstanding future with
`wait_for(0ms)` in a loop and then slept 1 ms, which cost 0.372 s of CPU across 11,535 calls and put
up to a millisecond in front of every completion. Workers now push their result onto a mutex and
condition-variable queue and signal it, so the dispatcher blocks until there is something to
collect. `run_bounded_async` takes the task rather than a future, since every call site was already
wrapping its work in `global_thread_pool().submit(...)`, and moving that inside is what lets the
wrapper attach the signal.

The scope's CPU falls from 0.372 s to about 0.21 s. Its wall time barely moves and neither does the
run's, which is the useful part of the result: the dispatcher was mostly blocked on work that had
not finished, not on poll granularity. This pipeline is throughput-bound, so removing the poll buys
CPU rather than latency.

**Five duplicated `execute_and_capture` implementations.** `src/runner/spot.cpp`,
`src/runner/black.cpp`, `src/runner/ltlfilt.cpp`, `src/runner/ganak.cpp` and
`src/runner/formaliser.cpp` each carry a near-identical copy. This is not a performance problem in
itself, but every fix to the spawn path has to be made five times, as both the `posix_spawn` change
and the `O_CLOEXEC` fix did.

## Validation under ThreadSanitizer

The batcher and the completion queue are both new concurrency, so the branch was checked under the
`tsan` preset as well as the normal suite. The check earned its keep immediately: a whole `counter`
run over `fsm` reported 41 data races, all of them in the new batching code.

The cause is worth recording, because guarding it the obvious way did not fix it.
`run_ltlfilt_batch` accumulated into `LtlfiltStats::total_cpu_s`, a static that `simplify_ltl`
already updates under its cache lock. Giving the batch path a mutex of its own left two locks
protecting one variable, which is no protection at all, and ThreadSanitizer went on reporting it.
The batch now hands the child's CPU time back through an out-parameter and the leader books it
against its own call, so there is one writer under one lock.

After that fix: a full run is clean (0 races, exit 0), and the whole suite passes under `tsan`,
38 of 38 with no warnings. No pre-existing race surfaced on either workload.

Building the `tsan` preset in a worktree is cheap if `build-tsan/third_party` is symlinked to a
tree that already has Spot and black built; otherwise it rebuilds both from source.

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
felt like the obvious next step, and the fix returned 3%. The same pattern repeated three times more
— once in a whole-run figure taken from a single sample, once in a worker pool built on the
assumption that a line in means a line out, and once in the claim that batching cost no wall time.
That last one compared two sets of runs taken at different times instead of interleaving them. It is
the single-sample mistake again, made a second time after it had already been caught once.
