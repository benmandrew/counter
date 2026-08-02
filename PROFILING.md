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
- the cost of in-process simplification through a linked `libspot` on a real corpus — the 1791
  distinct formulae an `fsm` gen20/pop1000 run simplifies cost 0.05 s in process, the same as one
  batched `ltlfilt` exec over them;
- that the per-call timeouts on `ltl2tgba` and `ltlsynt`, and `ltlsynt`'s memory cap, work only
  because those tools are separate processes that can be killed;
- that in-process simplification is a drop-in at simplification level 3, and only at that level —
  byte-identical to `ltlfilt --simplify` on all 1791 real formulae and on 5000 random ones — but is
  only thread-safe behind a single process-wide lock, since the state it contends on is SPOT's
  global parser and formula-interning tables rather than anything per-simplifier;
- that the in-process path is now implemented, and that whether it beats spawning depends on the
  workload rather than on the change — 0.15 ms to simplify an `fsm` formula against 24 ms for a
  `lift` one, which is why the lock is waited on only for as long as a spawn would cost;
- that with that budget every specification in the repository gets faster and none regresses, by
  between 10% and 35% of whole-run wall time;
- byte-identical output across all 12 `repair_N.json` files before and after every change.

Whole-run wall-clock improvement is established, by the method this document insists on — an *A/B
test* whose two configurations run alternately in one session, because two sets of runs taken at
different times cannot separate a real effect from run-to-run spread. Measured end to end on `fsm`
gen20/pop1000, against the same binary configured to spawn both tools as it used to:

| | before | after | |
|---|---|---|---|
| one run, three repetitions | 6.47 s | 5.10 s | 21% sooner |
| four concurrent runs, five repetitions | 12.50 s | 8.98 s | 28% sooner |
| four concurrent runs as a campaign runs them, five repetitions | 12.36 s | 7.00 s | 43% sooner |

Both are medians, and neither pair of distributions overlaps. Both rows are the in-process path as
it now stands, with the 8 ms lock budget described under "Waiting only as long as a spawn would
cost". Earlier versions of this section reported 6.54 s to 4.87 s and 12.46 s to 8.33 s, both taken
before that budget existed; those pairs are superseded, and the small loss against them is the
budget's price, paid to remove a regression elsewhere. Every figure in this section was re-taken
against the code as it finally stands, so none of them predates a later change. Almost all of the single-run gain is the
simplifier; the translator adds CPU headroom rather than latency, which is why its own contribution
shows up in the concurrent row and not the first.

The third row is the one that matters most in practice, because it is the shape a campaign runs in.
`scripts/run_experiments.py` sets `parallel` per job so that jobs times workers comes to about the
core count, so the comparison is four jobs with pools of five against what those runs previously
did: pools of twenty each, because the key was ignored, and both tools spawned. That combines the
two independent findings on this branch, and they compound -- 43% is close to the two effects
multiplied rather than either alone. The batcher's own wall cost stands as measured,
and now applies only when the exec engine is selected explicitly.

One specification is not enough to claim a general result, so the comparison was repeated on every
other FRETISH example in the repository and on two TLSF ones, three interleaved repetitions each,
same seed and configuration. The TLSF pair is there because `lift` is what exposed the regression
recorded under "A regression the other specifications found", and a table that reported only the
specifications which happened to agree would be claiming generality from the easy cases:

| specification | spawning both | in process, budgeted | |
|---|---|---|---|
| `fsm` | 6.47 s | 5.10 s | 21% sooner |
| `fsm-timing` | 10.19 s | 8.86 s | 13% sooner |
| `fsm-combined` | 16.00 s | 12.76 s | 20% sooner |
| `takeoff` | 4.87 s | 4.32 s | 11% sooner |
| `minepump` (TLSF) | 3.06 s | 1.87 s | 39% sooner |
| `lift` (TLSF) | 23.73 s | 20.96 s | 12% sooner |

Medians again, and no pair overlaps. The gain is smallest on `takeoff` and `fsm-timing`, which is
what should be expected: the saving is per exec, so it is proportionally smaller on a workload that
spends more of its time inside `black` and `ltlsynt`, neither of which moved. All six produce
byte-identical repairs either way, which is the more important half of the result.

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

Because the right setting depends on how the machine is being used rather than on anything intrinsic
to the code, it is a configuration key rather than a constant: `[runtime] ltlfilt_batchers`, default
four, with zero switching batching off entirely and restoring one exec per call.

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

The same holds on the TLSF path, with smaller numbers. Single runs of three TLSF specs, medians of
two repetitions each:

| spec | wall | system CPU | minor faults |
|---|---|---|---|
| minepump | +4.8% | −19.2% | −11.8% |
| detector | +14.4% | −27.2% | −21.1% |
| lift | +10.1% | −33.2% | −22.2% |

The wall cost is larger here than on FRETISH and so is the CPU saving. Four concurrent runs of
`detector.tlsf` still come out ahead — 21.41 s baseline against 20.46 s batched by median, every
one of three repetitions favouring the batched build — but by 4.4% rather than the 12.6% seen on
`fsm`. That is the expected shape: TLSF runs are dominated by `ltlsynt`, which is not batched, so
there is proportionally less `ltlfilt` startup to recover.

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

## Linking `libspot` in process

Removing the process boundary was an untried idea until now. A *spike* — a standalone program
written to answer one question and then thrown away — settles the measurement half of it. The spike
compiles against the `libspot` the build tree already produces (`-I<spot>/include -lspot -lbddx`)
and calls `spot::parse_infix_psl`, `spot::tl_simplifier::simplify` and `spot::str_psl` in sequence,
which is the in-process equivalent of `ltlfilt --simplify`. It links and runs with no dependency
beyond what the build already makes.

Two measurement errors in the first version of this section were found later, and both of them
changed the answer.

The first is the simplification level. The spike constructed `spot::tl_simplifier` with its default
options, and `ltlfilt --simplify` is not the default. `ltlfilt` builds
`spot::tl_simplifier_options(level)`, and its `--simplify` flag sets level 3, which additionally
enables `containment_checks` and `containment_checks_stronger`. The library default has both off.
Over 5000 random formulae — `randltl -n 5000 --tree-size=5..40 --seed=1` over six atoms — the
default options disagree with `ltlfilt --simplify` on 259 of them, about 5%. The earlier claim of
byte-identical output on 300 formulae was therefore true only of that sample and only at the wrong
level. It was luck, not equivalence.

The second is the corpus. The "200 formulae" behind the 1800 ms / 20 ms / 2.25 ms table, and behind
the 0.011 ms per formula read off it, were not formulae from a run at all: the file held the same
trivial formula, `a & b`, repeated. Level 3 is far more expensive than the default, because the
containment checks build automata. On the 5000 random formulae, in process at level 3 takes 37.3 s
and a single batched `ltlfilt` exec over the same 5000 takes 37.2 s — the same figure, because at
that difficulty the simplification work dominates and process startup is noise.

The honest measurement needs a real corpus. Every `simplify_ltl` cache miss of an `fsm`
gen20/pop1000 run was dumped, which gives 1791 distinct formulae with a median length of 138
characters. On those, in process at level 3 and one batched `ltlfilt` exec both take 0.05 s. The
in-process output is byte-identical to `ltlfilt --simplify` on all 1791 real formulae and on all
5000 random ones.

Real formulae are cheap to simplify. The cost that mattered was never the simplification, it was
the process. That sharpens the case for going in process rather than weakening it: essentially the
entire measured cost is startup, and in-process work does not pay it.

The blocker is not the linking. It is the timeout, and it splits the target in two.

`ltlfilt --simplify` has no timeout in this codebase today: `simplify_ltl` never passes one. Moving
it in process therefore gives up nothing that currently exists. `ltl2tgba` and `ltlsynt` both have
per-call timeouts (`g_ltl2tgba_timeout_ms`, `g_ltlsynt_timeout_ms`), and `ltlsynt` also has a
concurrency cap that bounds peak memory. Those exist because *determinization* and synthesis can run
unbounded on formulae the search generates, and they work only because the work sits in a separate
process that can be killed. In process there is nothing to kill. C++ has no cancellation, so a
runaway call leaves the choice between letting the thread run forever and killing the whole process.

So this is two targets, not one.

Thread safety was the remaining unknown, and it has now been tested rather than assumed. SPOT's
headers make no claim either way, and scoring runs on every pool worker at once, so the spike was
extended to drive 16 threads over 400 formulae each under ThreadSanitizer, in four configurations:

| configuration | result |
|---|---|
| one simplifier per thread, no lock | 6 data races, then SEGV |
| one shared simplifier, no lock | SEGV |
| one shared simplifier, global lock | exit 0, no races, no mismatches |
| one simplifier per thread, global lock | SEGV |

The naive expectation — that giving each thread its own simplifier avoids sharing — is wrong, and
the race reports say why. The contended state is not inside `tl_simplifier` at all. It is
process-global and sits underneath: SPOT's Bison and Flex parser globals (`tlyyfree`, the parser's
`value_type`), and the global `robin_hood` table that interns `fnode` formula nodes. A per-thread
simplifier still reaches all of it.

That the fourth row also crashes is the sharp part of the result. With every `simplify` call under
the lock, the only thing left running unserialised is *constructing* and destroying the per-thread
simplifiers — so construction itself is unsafe, not just use.

The rule that follows is therefore narrow and worth writing down: one simplifier, constructed once,
with every libspot call serialised behind a single process-wide lock. Nothing per-thread, and no
unlocked construction.

Serialising costs almost nothing at this scale. The 1791 distinct formulae a whole run simplifies
take 0.05 s in process even fully serialised, against the 11.82 s the same run spends in
`ltlfilt/batch-exec`.

## The in-process simplifier

The exec path was profiled first, so the change has something to be measured against. `fsm` at 20
generations and population 1000, seed 1:

| site | calls | wall |
|---|---|---|
| `ltlfilt/simplify_ltl` | 107,559 | 29.42 s |
| `ltlfilt/batched-request` | 2367 | 29.25 s |
| `ltlfilt/batch-exec` | 1494 | 11.82 s |

Of the 107,559 `simplify_ltl` calls, 105,192 hit the cache and 2367 missed. Average batch size is
1.58.

Two things follow from that, and both are worth saying. Coalescing barely fires inside a single
process: 2367 misses became 1494 execs. The batcher was measured across four concurrent `counter`
processes, where it does help; within one process the scoring threads rarely collide closely enough
to fill a batch. And the 11.82 s is essentially all startup, because the same 2367 formulae measure
about 0.05 s of actual simplification. Callers see 29.25 s rather than 11.82 s, since followers
block waiting on a leader's exec.

`spot_simplify` (`include/runner/spot_simplify.hpp`, `src/runner/spot_simplify.cpp`) implements the
rule above. One shared `spot::tl_simplifier`, built with `spot::tl_simplifier_options(3)` and
constructed inside the same process-wide mutex that guards every call. A formula that does not
parse returns `std::nullopt` and the caller returns it unchanged, which is what the exec path did
with a formula `ltlfilt` could not parse. `cmake/spot.cmake` now exports `SPOT_INCLUDE_DIR`,
`SPOT_LIB_DIR` and `SPOT_LIBRARIES`, and `counter_fitness` links `libspot.so` and `libbddx.so` with
an rpath back into the build tree.

`[runtime] simplify_engine` chooses between `"libspot"`, the default, and `"ltlfilt"`. The exec path
is kept rather than deleted for two reasons: it is the A/B lever the in-process path was measured
against, and it is the escape hatch if a future SPOT ever disagrees with its own command-line tool.
`ltlfilt_batchers` now takes effect only under `"ltlfilt"`.

The result was measured the way this document insists on — interleaved, alternating the two engines
within one session, seven repetitions each, same seed, same config, same spec (`fsm` gen20/pop1000):

- libspot: 4.41, 4.45, 4.51, 4.52, 4.68, 4.71, 4.80 s, median 4.52 s;
- ltlfilt: 6.04, 6.41, 6.50, 6.57, 6.59, 6.76, 6.89 s, median 6.57 s.

The two distributions do not overlap: the slowest in-process run is faster than the fastest spawning
run. That is about 31% of whole-run wall time. Both engines produce identical repair output from the
same seed, with `diff -r` over the two output directories clean.

`test/runner/spot_simplify_tests.cpp` is registered as the ctest suite `counter_tests.spot_simplify`.
It pins agreement with `ltlfilt --simplify` over a set of formulae; pins the level-3 requirement
specifically, using `b | G(Fe U Gc)`, which level 3 simplifies to `b | (Fe U Gc)` while the default
options leave the `G` in place; pins the decline path for an unparseable formula; and drives eight
concurrent callers to check that they agree.

## The in-process translator

The counting path spawned `ltl2tgba -D -S -H -f <formula>` once per formula. Dumping every cache
miss on that path gives the 241 distinct formulae an `fsm` gen20/pop1000 run translates, and
translating those two ways gives the split:

- serialised in process: 0.03, 0.04, 0.03 s;
- one exec per formula, as the code did: 2.06, 2.51, 2.68 s.

That is about 80 times more process than work. Startup dominates again. It is also why serialising
every translation behind the libspot lock still costs less than the execs it removes.

Two implementation details are worth recording, because the obvious choice is wrong in each case.

- Each call translates against a *fresh* `bdd_dict`. Sharing one across calls is no faster — 0.04 s
  over the same 241 formulae — and it renumbers atomic propositions, because a dictionary carries
  over the propositions earlier formulae registered. A fresh one reproduces the tool's numbering.
  The reader for the *Hanoi Omega-Automata* (HOA) format resolves guards through the
  atomic-proposition name list, so the renumbering would have been internally consistent, and
  therefore invisible until something else depended on it.
- Output matches the tool exactly apart from the `name:` line, which `ltl2tgba` fills with its own
  simplified rendering of the formula and which nothing in this project reads.

The move is conditional. It happens only when `ltl2tgba_timeout_ms` is zero, which is the default. A
per-call deadline in this project is enforced by killing the process doing the work; in process
there is nothing to kill, and C++ cannot cancel a running call. A configured timeout therefore keeps
the exec rather than silently losing the guarantee it was asked for. This is the split that "Linking
`libspot` in process" predicted, resolved rather than dodged: the tool moves where the guarantee
does not exist, and stays where it does. `ltlsynt` still cannot move at all, for the same reason
plus its memory cap.

One real surprise came out of this, and the existing `spot_runner` test suite is what caught it.
Spot 2.15.1 refuses to print the *universal automaton* it builds for a tautology, reporting that the
automaton is complete while its `prop_complete()` flag is unset. That defect had been recorded in
this project as an `ltl2tgba` *binary* bug — exit 2 with that message on stderr. It is not. It is in
the *library*, and in process it surfaces as a thrown exception instead.
`SpotTranslation::m_tautology_print_bug` reports it, so the caller substitutes the universal
automaton exactly as it already did for the exec's exit 2, and a genuinely-true formula still does
not count against the run's scoring-failure tolerance.

Results, measured interleaved as this document insists. Single run, `fsm` gen20/pop1000, seven
repetitions of each engine, alternating:

- in process: 4.55, 4.57, 4.74, 4.80, 4.80, 4.82, 4.89 s, median 4.80 s;
- spawning: 4.52, 4.53, 4.58, 4.60, 4.64, 4.80, 4.83 s, median 4.60 s.

These two distributions overlap almost completely, and the in-process median is the slower of the
two. There is no wall-clock gain for a single run. Total CPU does fall, from about 39.7 s to about
34.2 s of user plus system time, roughly 14%.

Four concurrent runs, five repetitions each, alternating:

- in process: 8.246, 8.285, 8.287, 8.311, 8.356 s, median 8.287 s;
- spawning: 8.914, 8.916, 9.079, 9.083, 9.234 s, median 9.079 s.

Those do not overlap: the in-process build finishes about 8.7% sooner. This is the same shape as the
batcher's result and it has the same cause — the CPU the exec path burns demand-paging its children
is the CPU a neighbouring `counter` needs. The case for this change rests on the concurrent
measurement, not on the single-run one.

Repairs are byte-identical to the exec path, the suite passes 39 of 39, and ThreadSanitizer reports
no races on a full run or on the suite.

## The lock is now the limiting factor

This is the negative result that explains why single-run wall time did not move. Profile of an `fsm`
gen20/pop1000 run, seed 1, after the change:

- `proc/read`: 1472 calls, 32.19 s wall;
- `ltlfilt/libspot-simplify`: 2129 calls, 8.28 s wall, 0.32 s CPU;
- `spot/libspot-translate`: 475 calls, 7.67 s wall, 0.73 s CPU.

The exec count has come down in stages across this branch: 3442 calls and 42.31 s of `proc/read`
before any of it, 1901 calls and 37.13 s after simplification moved, and 1472 calls and 32.19 s now.

The two in-process scopes are the finding. Together they account for roughly 16 s of caller wall
time for roughly 1.05 s of actual work. Before translation was added, `ltlfilt/libspot-simplify`
alone was 1874 calls, 1.68 s of wall against 0.29 s of CPU. Putting translation on the *same* lock
pushed simplification's wait from 1.68 s to 8.28 s. The lock is held for about 1.05 s of a 4.6 s
run, and twenty scoring threads queue behind it.

So the process boundary has been traded for a lock, and on a single run the trade is roughly even.
The bottleneck moved rather than vanished. What was gained is real, but it is CPU rather than
latency, which is why the concurrent measurement is the one that shows a win.

These are single-run profile figures, and wall time attributed to a scope under lock contention is
noisy between runs. The interleaved medians above are the reliable numbers, not these.

Spot offers `--enable-pthread`, and it does not help. It activates parallel variants of some
algorithms; it does not make the parser globals or the formula-interning table reentrant, which is
what the lock exists for. Removing the lock would need Spot to change, not this project.

## Ranked targets

**Batched tool calls — done for `ltlfilt`.** The largest lever on CPU, and the lever is eliminating
per-spawn startup rather than keeping a process warm. It buys about 19% of total CPU and costs about
5% of wall time; the design and the numbers are in "The batched simplifier" above. Two parts of this
target are untouched. `black` accounts for 818
execs on this workload and has no batch mode of its own, so nothing here applies to it. `ltlsynt` is
still one exec per call; `ltl2tgba` no longer is, but by moving in process rather than by batching,
so nothing in this entry applies to it either.

**Link `libspot` in process — done for both tools that can move.** SPOT
is already built from source, and `libspot.so` and its headers sit in the build tree.
Simplification now runs in process unconditionally, behind one lock, and takes `fsm` gen20/pop1000
from a median of 6.57 s to 4.52 s of whole-run wall, about 31%. The 1791 distinct formulae such a
run simplifies cost 0.05 s in process against the 11.82 s the exec path spent in
`ltlfilt/batch-exec`, nearly all of which was startup; the design and the numbers are in "The
in-process simplifier" above. Counting-path translation followed it, but only when no per-call
deadline is configured, which is the default; "The in-process translator" has that argument and its
measurements. `ltlsynt` cannot move as things stand, because its per-call timeout and its memory cap
both work by killing a separate process. Replacing the ability to abandon a call is a design
question rather than an optimisation, and it is the thing to answer before that last tool moves.

The top remaining in-process target is the lock the two moved tools now share. It is held for about
1.05 s of a 4.6 s run, and the wait it imposes on callers is roughly 16 s of scope wall summed
across twenty scoring threads; "The lock is now the limiting factor" has the figures. `black` is not
part of any of this: 890 calls and 24.9 s on this workload, with no batch mode of its own and no
library to link, so nothing here applies to it.

**Thread-pool oversubscription — fixed.** `[runtime] parallel` was documented as the thread pool
size and never reached the pool, so every run built a full-width one no matter what a campaign
asked for. Honouring the key takes four concurrent `fsm` gen20/pop1000 runs from a median of 8.40 s
to 6.94 s, about 17% sooner for the same work. The scaling table in "The thread pool ignored its own
setting" says where that comes from: twenty workers buy 4.3 times the throughput of one in process,
so campaign throughput comes from running more jobs with smaller pools rather than from widening any
single run.

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

**Duplicated `execute_and_capture` implementations — done for four of the five.**
`src/runner/spot.cpp`, `src/runner/black.cpp`, `src/runner/ltlfilt.cpp` and `src/runner/ganak.cpp`
each carried a near-identical copy. That is not a performance problem in itself, but it is why both
spawn-path fixes on this branch had to be repeated per copy — `posix_spawn` twice, `O_CLOEXEC` five
times — and why one of them was easy to miss.

They now share `run_subprocess` (`include/runner/subprocess.hpp`). The copies differed on exactly
two axes, a timeout and whether the child must die with its parent, so both became options.
`spot.cpp` is the only caller asking for the second, which is why it alone keeps `fork()`. Net
effect on the runners is 507 lines removed for 21 added, against a roughly 190-line shared module.

Two callers are deliberately left out. `src/runner/formaliser.cpp` keeps one long-lived `node`
child rather than one per call, so it is a different shape entirely. `run_ltlfilt_batch` drives a
bidirectional pipe pair, which `run_subprocess` does not cover — it only captures output — so it
spawns and reaps for itself. Folding either in would mean widening the shared interface to fit one
caller each.

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

The in-process simplifier was checked the same way, and needed to be: it puts a linked library with
process-global state under every scoring thread, which is exactly the shape the spike showed can
crash. A full `counter` run on `fsm` with `simplify_engine = "libspot"` reports 0 races and exits 0,
and the suite passes 39 of 39 under `tsan` with no warnings, the extra suite being the concurrency
test that drives eight callers through `spot_simplify` at once. This confirms on a real workload
what the spike established in isolation: one shared simplifier behind one process-wide lock is
enough, and nothing else is.

Building the `tsan` preset in a worktree is cheap if `build-tsan/third_party` is symlinked to a
tree that already has Spot and black built; otherwise it rebuilds both from source.

## The thread pool ignored its own setting

The intent was to measure how a run scales with worker count, by varying `[runtime] parallel` from 1
to 20. The result looked like a finding. Wall time barely moved, and more workers were slightly
*worse*: 4.24, 4.33, 4.80, 5.28 and 4.87 s at 1, 2, 4, 8 and 20 on the in-process path, and a flat
6.93, 6.66, 6.88, 6.78, 6.45 s on the spawning one. Read at face value, that says the run is bound
by something serial and parallelism is not the lever.

It says nothing of the kind. `global_thread_pool()` was hard-coded to
`std::thread::hardware_concurrency()` and never read `Config::parallel` at all. The key reached only
`max_in_flight` in `score_population`, the bounded-async in-flight window. The experiment therefore
varied the window, not the pool, and every one of those runs used a full-width pool of twenty. The
lesson is the ordinary one: a knob that produces no effect is as likely to be disconnected as to be
unimportant, and checking that it is wired takes less time than interpreting the result.

Both `schemas/config-schema.json` and `example-config.toml` describe the key as "thread pool size",
so this was a documented promise the code did not keep.

It matters most where it was relied on. `scripts/run_experiments.py` writes `parallel = k` into each
level's config for exactly this purpose — its own comment says it caps each run's internal thread
pool so that jobs times parallel comes to about the core count. That never happened. On a 20-core
machine a campaign with eight concurrent jobs ran eight full-width pools, so 160 workers contended
for 20 cores. This does not put any archived campaign's *results* in doubt, but their recorded
timings are timings of an oversubscribed machine.

The fix is to read the key when the pool is first built, with zero meaning the hardware concurrency,
so a caller that never sets it is unaffected.

What it buys, measured on `fsm` gen20/pop1000, four concurrent runs, five interleaved repetitions:

- pools of five, which is what `parallel = 5` now gives: 6.849, 6.930, 6.938, 6.988, 7.023 s, median
  6.94 s;
- full-width pools, which is what ran before: 8.377, 8.382, 8.398, 8.518, 8.524 s, median 8.40 s.

The distributions do not overlap: about 17% sooner, for a machine doing the same work.

With the key honoured, the scaling measurement can finally be taken, and it is the one that should
have been there all along. One run, workers 1 through 20:

| workers | in process | spawning |
|---|---|---|
| 1 | 18.69 s | 39.22 s |
| 2 | 9.23 s | 18.99 s |
| 4 | 6.00 s | 10.08 s |
| 8 | 5.01 s | 8.42 s |
| 20 | 4.36 s | 6.45 s |

Two things are worth reading off it. Neither path scales anywhere near linearly — twenty workers buy
4.3 times the throughput of one in process, and 6.1 times spawning — so a campaign is better served
by many small pools than by one wide one, which is precisely what the broken key was trying to
arrange. And the in-process path scales the worse of the two, which is the global libspot lock
showing up exactly where the previous section predicts it would.

The single-worker row is the cleanest comparison this document has of what removing the execs buys,
because nothing contends at one worker: 18.69 s against 39.22 s, a little over half.

Output is identical across pool sizes 1 and 20 for the same seed, so the key changes cost and not
results.

## A regression the other specifications found

Every measurement up to this point was taken on `fsm`. Repeating the comparison on the other
examples in the repository found that one of them had got materially worse. On the TLSF
specification `lift`, three interleaved repetitions each:

- spawning both: 23.57, 24.35, 25.13 s, median 24.35 s;
- both in process: 27.76, 27.81, 27.94 s, median 27.81 s.

Non-overlapping, and about 14% *slower*. Crossing the two changes separately identifies which one,
and it is not the one the previous section would suggest:

| configuration | median |
|---|---|
| both spawned | 24.35 s |
| simplification spawned, translation in process | 24.32 s |
| simplification in process, translation spawned | 28.33 s |
| both in process | 27.81 s |

Translation is neutral on `lift`. Simplification is the whole of the regression.

The profile says why, and the number is stark. On `lift`, `ltlfilt/libspot-simplify` records 1005
calls and 23.93 s of CPU. That is 23.8 ms per call, against 0.15 ms per call on `fsm` — a factor of
160. Level 3's containment checks build automata, and `lift`'s formulae make that expensive.
Serialising 23.9 s of CPU behind one lock puts almost the whole of a 28 s run on a single thread,
where separate `ltlfilt` processes had been running the same work in parallel.

The general lesson is worth stating plainly. Replacing a process with a lock trades parallelism for
startup, and which side of that trade wins depends on how expensive the work is — which is a
property of the workload, not of the code. A change measured on one specification had been read as
a property of the change.

## Waiting only as long as a spawn would cost

Neither fixed choice is right, and the cost of a formula is not knowable before simplifying it.
What is knowable is what the alternative costs. Spawning is about 8 ms, in line with the per-spawn
tax measured at the top of this document, and it does not vary with the formula.

So the rule needs no prediction. A caller waits for the libspot lock for as long as spawning would
have taken, and if the lock has not come free by then, it spawns — because past that point spawning
is the cheaper option by definition. `spot_try_simplify` takes that budget and reports
`m_lock_busy` when it expires, and `simplify_ltl` falls through to the batched exec path it already
had. The budget is 8 ms, and the mutex is a `std::timed_mutex` so the wait can be bounded.

This is only sound because the two paths produce identical output — established over 5000 random
formulae, 1791 real ones, and now six whole specifications — so which path a given call takes is
not observable in the result. If they could ever disagree, a contended run would stop reproducing,
and the fallback would be unsafe rather than merely uneven.

Results, three interleaved repetitions each, medians, against the same binary configured to spawn
both tools:

| specification | spawning both | in process, budgeted | |
|---|---|---|---|
| `fsm` | 6.52 s | 5.15 s | 21% sooner |
| `fsm-timing` | 9.96 s | 8.96 s | 10% sooner |
| `fsm-combined` | 15.86 s | 12.95 s | 18% sooner |
| `takeoff` | 4.90 s | 4.40 s | 10% sooner |
| `minepump` (TLSF) | 3.00 s | 1.96 s | 35% sooner |
| `lift` (TLSF) | 24.27 s | 20.08 s | 17% sooner |

No specification regresses, and all six produce byte-identical repairs.

`lift` is the interesting row, because 20.08 s beats *both* fixed choices — 24.27 s always spawning
and 27.81 s always in process. Taking the cheap path when it is free and the parallel one when it
is not is better than either, which is what the budget buys. The cost is visible on `fsm`, where
5.15 s is a little worse than the 4.87 s an unbudgeted in-process path reached, because some calls
now pay a wait before spawning anyway. Trading 0.3 s on the best case to remove a 3.5 s regression
on the worst is the right way round.

### Checking the budget against a sweep

The 8 ms figure is derived rather than tuned — it is what a spawn costs — so it is worth asking
whether the derivation lands anywhere near the best value. Sweeping it over the two specifications
that disagree most, one run each:

| budget | `fsm` | `lift` |
|---|---|---|
| 0 ms, never wait | 5.47 s | 20.15 s |
| 2 ms | 5.41 s | 19.90 s |
| 8 ms | 5.19 s | 19.28 s |
| 32 ms | 4.84 s | 21.80 s |
| unbounded, always wait | 5.22 s | 27.92 s |

These are single runs rather than medians, so small differences between neighbouring rows should not
be read closely; the shape is what matters, and it is clear enough.

Both extremes are worse than the middle, which is the useful part. Never waiting gives up the
in-process path even when the lock is free and is the worse choice on both. Always waiting is
catastrophic on `lift` and no better on `fsm`. Between them, 8 ms is the best value measured on
`lift` and within 7% of the best on `fsm`, where 32 ms edges it — and 32 ms costs `lift` 13%. The
derived value therefore sits close to the empirical optimum on both, without having been fitted to
either, which is the property worth having: a tuned constant would be tuned to whichever
specification happened to be measured.

### The translator gets the same budget, without a measurement to justify it

Translation kept blocking on the lock for a while after simplification stopped, and that asymmetry
was an accident rather than a decision: both calls contend on the same lock, so both can be starved
the same way. Determinization can be arbitrarily expensive, so a specification whose translations
cost what `lift`'s simplifications cost would serialise exactly as `lift` did.

It is worth being clear that this change buys nothing measurable. Three interleaved repetitions on
`fsm`, `lift` and `fsm-combined` overlap with the blocking translator in every case. It is kept
because the failure mode was measured on the other path and the two paths are the same shape, not
because anything here got faster — a change with no measured benefit should say so rather than
borrow the credit of the one next to it.

## Where a run's wall time goes now

The `--dashboard` progress log already times every pipeline stage, so the breakdown below needs no
extra instrumentation: run with `--dashboard` and sum the `stage` records in `progress.jsonl` by
name. For `fsm` gen20/pop1000 at twenty workers, over the whole run:

| stage | total | share |
|---|---|---|
| `weakening` | 1.83 s | 40.3% |
| `score` | 1.76 s | 38.8% |
| `vacuous-assumptions` | 0.60 s | 13.2% |
| `breed` | 0.17 s | 3.6% |
| `select` | 0.11 s | 2.5% |
| the remaining seven stages | 0.05 s | 1.6% |

The stages sum to 4.53 s, which is the run, so this accounts for all of it rather than a sample of
it.

Two things follow. Scoring is no longer the largest stage — `weakening` is, and it is not far off
scoring and `vacuous-assumptions` combined. Both of those are `black` filters rather than fitness
work, which puts the majority of a run's wall time in the satisfiability checker rather than in the
search. And every stage above one percent is already dispatched through the pool, so the sublinear
scaling recorded above is not an unparallelised stage waiting to be found; it is contention and
per-stage dependency inside stages that are already parallel.

That also settles where `black` sits in the ranking. It is the tool this workload now spends most of
its time inside, it has no batch mode and no library to link, and the two stages that call it are
the two largest. Reducing the number of calls is the lever left, not making each one cheaper.

## What the debug preset caught

This branch was verified under `relwithdebinfo` and `tsan` throughout, and both were the wrong
place to look for two of its defects. The `debug` preset is the only one with AddressSanitizer and
UndefinedBehaviorSanitizer on, and running it at the end failed eight tests and one build.

The build failure is a consequence of linking `libspot`. `counter_fitness` names
`libspot.so` and `libbddx.so` as link inputs by absolute path, and `ExternalProject_Add` had not
declared them as `BUILD_BYPRODUCTS`. `add_dependencies` orders the targets but does not tell Ninja
what produces a file, so a tree where Spot was not already installed failed with "missing and no
known rule to make it". Every tree used during the work symlinked a `third_party` that was already
built, which is exactly why it went unnoticed.

The eight test failures are a LeakSanitizer report against the profiler. Its `Site` objects are
deliberately never freed, because a worker thread can still be inside a scope when static
destruction begins. The registry holding them was not: a function-local static vector, whose buffer
was freed at exit, orphaning every `Site` a moment before the leak check ran. The fix is not to stop
leaking but to keep the leak *reachable* -- holding the registry and the interned names behind
pointers that live to exit. That distinction, between an allocation that is intentional and one that
is merely lost, is the whole of what LeakSanitizer is reporting.

Neither defect is about performance, and both were introduced by this branch. The lesson worth
carrying is the narrow one: a preset that is convenient to iterate under is not the one that checks
the work.

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
