# The pairwise conflict degree, and the 15 families it cannot separate

`Config::mrs_admission_order = "degree"` orders the guarantee parts of a specification by *pairwise conflict degree*, ascending, before the greedy *maximum realizable subset* (MRS) walk admits them. A part's degree counts the other parts it cannot be held with, over all n(n−1)/2 pairs. This report measures what that degree separates across the 38 *Temporal Logic Synthesis Format* (TLSF) examples under `examples/`, whether a better order exists, and what one would cost. The decision it records is to keep the pairwise degree unchanged.

Nothing here was pre-registered, so there is no `PLAN.md` beside this file and no decision rule that binds the result. Every figure comes from three ad-hoc tools vendored under `scripts/`, built against the library and using the shipping oracle from `tlsf_mrs_admission_order` in `src/tlsf/fitness.cpp`: a subset is kept when `ltlsynt` reports it realizable and `tlsf_is_not_well_separated` reports it well-separated, with an undecided verdict resolving as unrealizable. Provenance is `30e8dd2` on `main` for the degree and optimum measurements and `9258ef4` for the mutant and cost measurements, which sit on av2 (`avlab12`); the two commits differ in `include/config.hpp` and `src/tlsf/mutation.cpp` alone, and neither touches the MRS path.

## Conflict-degree groups, per example

Parts are numbered in the order `tlsf::split_guarantee_parts` returns them, PRESET then ASSERT then GUARANTEE. `queries` is the n + n(n−1)/2 `ltlsynt` calls the order costs. `index`, `degree` and `optimum` are the parts the greedy walk keeps under index order, under degree order, and the exact maximum found by descending-cardinality search.

| family | n | queries | conflict-degree groups (degree: parts) | groups | solo-unreal | index | degree | optimum |
|---|---|---|---|---|---|---|---|---|
| `amba` | 52 | 1378 | `0: 0-51` | 1 | — | 51 | 51 | 51 |
| `arbiter` | 5 | 15 | `0: 2` · `1: 0,1,3,4` | 2 | — | 3 | 3 | 3 |
| `arbiter-aurus` | 3 | 6 | `1: 0,1` · `2: 2` | 2 | — | 2 | 2 | 2 |
| `arbiter-handshake` | 5 | 15 | `0: 0-4` | 1 | — | 4 | 4 | 4 |
| `codesample-un1` | 6 | 21 | `0: 0-3` · `1: 4,5` | 2 | — | 5 | 5 | 5 |
| `codesample-un2` | 7 | 28 | `0: 0-4` · `1: 5,6` | 2 | — | 6 | 6 | 6 |
| `detector` | 7 | 28 | `1: 1-6` · `6: 0` | 2 | — | 1 | 6 | 6 |
| `detector-aurus` | 3 | 6 | `0: 2` · `1: 0,1` | 2 | — | 2 | 2 | 2 |
| `full-arbiter` | 16 | 136 | `0: 0-5,10-15` · `1: 7-9` · `3: 6` | 3 | — | 13 | 15 | 15 |
| `full-arbiter-aurus` | 17 | 153 | `0: 0-5,7-12,16` · `1: 13-15` · `3: 6` | 3 | — | 14 | 16 | 16 |
| `gyro-var1` | 9 | 45 | `0: 0-5,7` · `1: 6,8` | 2 | — | 8 | 8 | 8 |
| `gyro-var2` | 9 | 45 | `0: 0-3,5,6,8` · `1: 4,7` | 2 | — | 8 | 8 | 8 |
| `humanoid-458` | 11 | 66 | `0: 0-10` | 1 | — | 10 | 10 | 10 |
| `humanoid-503` | 18 | 171 | `0: 2,4-6,9-13,16` · `1: 0,1,3,7,8,14,15` · `7: 17` | 3 | — | 17 | 17 | 17 |
| `humanoid-531` | 16 | 136 | `0: 0-15` | 1 | — | 15 | 15 | 15 |
| `humanoid-741` | 30 | 465 | `29: 0-29` | 1 | 0-29 | 0 | 0 | 0 |
| `humanoid-742` | 35 | 630 | `0: 0-9,11-33` · `1: 10,34` | 2 | — | 34 | 34 | 34 |
| `lift` | 16 | 136 | `0: 0-15` | 1 | — | 13 | 13 | 15 |
| `lily02` | 3 | 6 | `1: 0,1` · `2: 2` | 2 | 2 | 2 | 2 | 2 |
| `lily11` | 1 | 1 | `0: 0` | 1 | 0 | 0 | 0 | 0 |
| `lily15` | 5 | 15 | `0: 0-4` | 1 | — | 3 | 3 | 4 |
| `lily16` | 9 | 45 | `0: 0-8` | 1 | — | 6 | 6 | 6 |
| `load-balancer` | 8 | 36 | `0: 2,3,6,7` · `1: 0,1,4` · `3: 5` | 3 | — | 7 | 7 | 7 |
| `load-balancer-aurus` | 9 | 45 | `0: 2,3,5,6,8` · `1: 0,1,4` · `3: 7` | 3 | — | 8 | 8 | 8 |
| `ltl2dba-r-2` | 1 | 1 | `0: 0` | 1 | 0 | 0 | 0 | 0 |
| `ltl2dba-theta-2` | 1 | 1 | `0: 0` | 1 | 0 | 0 | 0 | 0 |
| `ltl2dba27` | 1 | 1 | `0: 0` | 1 | 0 | 0 | 0 | 0 |
| `minepump` | 2 | 3 | `1: 0,1` | 1 | — | 1 | 1 | 1 |
| `pcar-v2-888` | 24 | 300 | `0: 0-23` | 1 | — | 23 | 23 | 23 |
| `prioritized-arbiter` | 11 | 66 | `0: 1-3,7-10` · `1: 4-6` · `3: 0` | 3 | — | 8 | 8 | 9 |
| `prioritized-arbiter-aurus` | 12 | 78 | `0: 1-7,11` · `1: 8-10` · `3: 0` | 3 | — | 9 | 9 | 10 |
| `rg1` | 4 | 10 | `1: 0,2` · `2: 1,3` | 2 | — | 2 | 2 | 2 |
| `rg2` | 2 | 3 | `1: 0,1` | 1 | — | 1 | 1 | 1 |
| `round-robin-arbiter` | 4 | 10 | `0: 2,3` · `1: 0,1` | 2 | — | 3 | 3 | 3 |
| `round-robin-arbiter-aurus` | 5 | 15 | `0: 1,2,4` · `1: 0,3` | 2 | — | 4 | 4 | 4 |
| `simple-arbiter` | 7 | 28 | `0: 4-6` · `1: 1-3` · `3: 0` | 3 | — | 4 | 6 | 6 |
| `simple-arbiter-aurus` | 5 | 15 | `0: 1,2,4` · `1: 0,3` | 2 | — | 4 | 4 | 4 |
| `takeoff-tlsf` | 4 | 10 | `0: 0,2` · `1: 1,3` | 2 | — | 2 | 3 | 3 |


Fifteen of the 38 families put every part in one group, so the sort has nothing to separate and returns index order. Eight of those hold three or more parts and sit at degree 0 throughout: `amba` (52 parts), `pcar-v2-888` (24), `humanoid-531` (16), `lift` (16), `humanoid-458` (11), `lily16` (9), `arbiter-handshake` (5) and `lily15` (5), for 138 parts in total. No pair of parts conflicts in any of them while the whole specification is unrealizable, so the conflict is at arity three or more and a pairwise count cannot see it. `humanoid-741` is the mirror case: all 30 parts are unrealizable alone, every degree reads 29, every rank collapses to n+1, and the walk keeps nothing.

A probe of the triples confirms where the arity sits. `lift` and `humanoid-531` have zero conflicting triples out of 560 each, and `arbiter-handshake` zero out of 10, while their walks still reject 3, 1 and 1 parts — so those conflicts need four parts or more. `lily15` (3 of 10), `lily16` (9 of 84) and `humanoid-458` (1 of 165) do carry triple conflicts, which a triple degree would separate and the pairwise one cannot.

## Where the degree separates

Eleven families carry a single part whose degree is at least 2, which the order defers past everything it blocks. These are the specifications the heuristic was written for. `lily02` is in the table for its degree and deferred for a different reason: its part 2 is unrealizable alone, so the solo rule ranks it last whatever its degree says.

| family | part | section | degree | formula |
|---|---|---|---|---|
| `arbiter-aurus` | 2 | GUARANTEE | 2 | `G((a) \| ((!(g1)) & (!(g2))))` |
| `detector` | 0 | GUARANTEE | 6 | `((((G(F(r_0))) & (G(F(r_1)))) & (G(F(r_2)))) & (G(F(r_3)))) <-> (G(F(g)))` |
| `full-arbiter` | 6 | ASSERT | 3 | `((!(g_0)) & (!(g_1))) \| (((!(g_0)) \| (!(g_1))) & (!(g_2)))` |
| `full-arbiter-aurus` | 6 | ASSERT | 3 | `(((!(g_0)) & (!(g_1))) & (true)) \| ((((!(g_0)) & (true)) \| ((true) & (!(g_1)))) & (!(g_2)))` |
| `humanoid-503` | 17 | GUARANTEE | 7 | `G(F(((leftmotor_0) & (leftmotor_1)) & (leftmotor_2)))` |
| `lily02` | 2 | GUARANTEE | 2 | `G((cancel) -> (X((!(grant)) U (go))))` |
| `load-balancer` | 5 | ASSERT | 3 | `((request_0) & (X(request_1))) -> (X(X((grant_0) & (grant_1))))` |
| `load-balancer-aurus` | 7 | GUARANTEE | 3 | `G(((request_0) & (X(request_1))) -> (X(X((grant_0) & (grant_1)))))` |
| `prioritized-arbiter` | 0 | ASSERT | 3 | `((!(g_0)) & (!(g_1))) \| (((!(g_0)) \| (!(g_1))) & (!(g_2)))` |
| `prioritized-arbiter-aurus` | 0 | ASSERT | 3 | `(((!(g_0)) & (!(g_1))) & (true)) \| ((((!(g_0)) & (true)) \| ((true) & (!(g_1)))) & (!(g_2)))` |
| `simple-arbiter` | 0 | ASSERT | 3 | `((!(g_0)) & (!(g_1))) \| (((!(g_0)) \| (!(g_1))) & (!(g_2)))` |

`detector` is the extreme: one part at degree 6 against six at degree 1, and deferring it takes the walk from 1 part of 7 to 6. The arbiter families share a shape, the mutual-exclusion `ASSERT` conjunct blocking three grant guarantees at once. Ten further families hold exactly one conflicting pair against a field of zeros, a signal too thin to change the walk on any of them.

## How much a better order could gain

Exact maxima were computed for all 38 families by searching subsets downward from n−1. Realizability is antitone in the guarantee set, so a superset of an unrealizable set needs no query, and the solo and pairwise verdicts prune the search. All 38 came back exact for 5,381 queries, the worst being `amba` at 24.3 s.

The degree order attains that exact maximum on 34 of 38 input specifications, index order on 29. Four families hold the whole remaining gap: `lift` (13 against 15), `prioritized-arbiter` (8 against 9), `prioritized-arbiter-aurus` (9 against 10) and `lily15` (3 against 4). Closing it on the input is cheap, since an order admitting a maximum subset first is optimal there by construction, at about 1.3× the degree order's queries.

That gain does not survive replay. The order is computed once and replayed on every candidate, so 300 mutants over 10 families were scored through `tlsf_status` under each scheme, at six chains of five mutations from the original with the shipping `Config`. Index order to degree order is 67 mutants better and 0 worse, which replicates the header's claim in `include/fitness/status.hpp`. Degree order to the input-optimal order is 13 better and 3 worse, moving the pooled mean 0.603 to 0.606.

## What the remaining gain costs

Scoring each candidate under several fixed orders and keeping the best does move the mean, by 0.0149 over 270 mutants on av2 with all 31 winning mutants gained and none lost. Redrawing those orders per candidate scores 0.6296 against the fixed portfolio's 0.6297 while costing 22% more cache misses, so a portfolio should be drawn once and replayed. Gating the extra orders on the first walk's result recovers most of the cost.

| config | score | wall_s | ltlsynt_s | tool% | misses | ×ltlsynt |
|---|---|---|---|---|---|---|
| degree alone | 0.6148 | 77 | 70 | 91 | 1362 | 1.00 |
| gate k=2 | 0.6297 | 136 | 128 | 94 | 4583 | 1.83 |
| gate k=3 | 0.6266 | 103 | 96 | 93 | 2797 | 1.37 |
| gate k=4 | 0.6208 | 92 | 85 | 92 | 2131 | 1.21 |
| ungated | 0.6297 | 504 | 496 | 98 | 6115 | 7.09 |

The gate opens when the degree walk keeps at most n−k of the candidate's own parts. Tool calls are 91% to 98% of wall time, and `black` contributes none of it: the component satisfiability checks resolve without an exec and hold at 259 misses across every scheme. Misses overstate cost by roughly a factor of two, because per-miss cost is 51 ms at the baseline, 28 ms at k=2 and 81 ms ungated — the extra orders query smaller subsets, and the gate skips the expensive ones.

## Decision

The pairwise conflict degree stays as the shipping order, unchanged. It is measurably right where it fires, it is free at 0.050% of a campaign's median run wall time, and the two alternatives measured against it are both refused. An input-optimal order buys 13 mutants of 300 and loses 3. A gated portfolio buys 0.0149 of mean score for 1.83× the realizability tool time, on an endpoint that is an intermediate rather than a repair outcome.

Two things this does not settle. The 15 single-group families get index order and will keep getting it, since their conflicts are higher-arity than any pairwise count reaches, and a triple degree costs C(n,3) queries against C(n,2). And every score here is the MRS status objective, which lower-bounds the true maximum realizable subset; whether a tighter bound converts into `implies_ideal` or yield has never been measured, and a campaign is what would decide it.

## Reproduction

The three tools sit in `scripts/` and build against the release tree:

```sh
clang++ -Iinclude -Isrc -isystem build-release/_deps/eigen-src \
  -isystem build-release/_deps/nlohmann_json-src/include \
  -isystem build-release/_deps/tomlplusplus-src/include \
  -Wall -O2 -DNDEBUG -std=c++17 experiments/2026-08-28-mrs-ordering/scripts/degrees.cpp \
  -o /tmp/degrees build-release/libcounter_fitness.a build-release/libcounter_core.a
/tmp/degrees --parallel=16 examples/*/spec.tlsf
```

`degrees.cpp` writes the solo verdict, the full pairwise matrix, the degree and the induced rank per part. `orders.cpp` adds the exact maximum and the order that attains it. `mutants.cpp` replays a stored order onto mutation chains and carries `--gate=k`, `--per-candidate`, `--degree-only` and `--restarts=N` for the cost table above.

Both the tools and this report exist because a pairwise count looked like it might be measuring nothing on much of the corpus. It measures nothing on 15 families of 38, and on the other 23 it recovers most of what an exact search would.
