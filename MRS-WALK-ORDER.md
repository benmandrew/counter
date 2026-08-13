# Admission order in the MRS walk

The greedy *maximal realisable subset* (MRS) walk in `status_score_mrs` (`src/fitness/status.cpp:38-60`) admits guarantee-side parts in specification index order. `include/fitness/status.hpp:102-107` states the rationale: greedy returns a maximal subset rather than a maximum one, so the order decides which maximal set is reached, and specification order makes the score a deterministic function of the candidate, which seed reproducibility requires. That leaves open how much the score is worth under a different order. Over 21 corpus specifications with 200 shuffles each, nine specifications move at all, and the corpus mean score rises from 0.702 under specification order to 0.747 under a random one.

## Method

A probe binary (`src/mrs_order_probe.cpp`, built in the throwaway worktree `.claude/worktrees/mrs-order` on branch `probe/mrs-order`, never committed to main) replays the same walk under an arbitrary admission order. It reuses the production paths: `tlsf::split_guarantee_parts` for the parts, `tlsf::build_part_subset` for the subset, and the subset oracle of `tlsf_status` (`src/tlsf/fitness.cpp:204-219`), which asks realisability with the well-separation fold behind it and resolves an undecided query as unrealisable. One deviation applies. Indices are sorted before the subset is lowered, so a given set of parts produces one formula string whatever order the walk reached it in. Realisability does not depend on conjunct order, so no verdict changes, and every order then shares `RealizabilityChecker`'s cache, which is what makes the sweep affordable.

The corpus is all 22 `examples/*/spec.tlsf`. Each specification gets the specification order plus 200 seeded shuffles (`std::mt19937`, seed 1), for 4200 shuffled walks over the 21 specifications that completed. The `ltlsynt` per-call timeout was 30s and no timeout fired anywhere, so every walk is fully decided. The 22nd, `amba`, is reported separately below.

Reproducibility was checked directly. The same seed produces byte-identical scores at 1, 4 and 8 threads, with only the wall-time column moving, and different seeds give different outcome distributions.

## How far the score moves

Twelve of the 21 specifications are completely insensitive, returning the specification-order score on all 200 shuffles: `arbiter` (5 parts, 0.600), `codesample-un1` (6, 0.833), `codesample-un2` (7, 0.857), `gyro-var1` (9, 0.889), `gyro-var2` (9, 0.889), `humanoid-458` (11, 0.909), `humanoid-531` (16, 0.938), `lily02` (3, 0.667), `minepump` (2, 0.500), `rg1` (4, 0.500), `rg2` (2, 0.500) and `round-robin-arbiter` (4, 0.750). Across all 4200 shuffled walks, 19.3% score above the specification order, 4.6% below and 76.1% equal.

The nine that vary, with parts, specification-order score, the minimum, mean and maximum over the 200 shuffles, the number of distinct score levels, the percentage of shuffles above and below specification order, and the number of distinct maximal sets the walk reached:

| spec | parts | spec | min | mean | max | levels | above | below | sets |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| detector | 7 | 0.143 | 0.143 | 0.761 | 0.857 | 2 | 86.5% | 0.0% | 2 |
| arbiter-aurus | 3 | 0.667 | 0.333 | 0.548 | 0.667 | 2 | 0.0% | 35.5% | 2 |
| simple-arbiter | 7 | 0.571 | 0.571 | 0.781 | 0.857 | 2 | 73.5% | 0.0% | 2 |
| prioritized-arbiter | 11 | 0.727 | 0.545 | 0.749 | 0.818 | 4 | 50.0% | 19.0% | 6 |
| load-balancer | 8 | 0.875 | 0.625 | 0.812 | 0.875 | 2 | 0.0% | 25.0% | 2 |
| takeoff-tlsf | 4 | 0.500 | 0.500 | 0.651 | 0.750 | 2 | 60.5% | 0.0% | 3 |
| arbiter-handshake | 5 | 0.800 | 0.600 | 0.772 | 0.800 | 2 | 0.0% | 14.0% | 4 |
| lift | 16 | 0.812 | 0.750 | 0.885 | 0.938 | 4 | 68.0% | 2.5% | 14 |
| full-arbiter | 16 | 0.812 | 0.812 | 0.896 | 0.938 | 2 | 66.5% | 0.0% | 2 |

Taking the best of the 200 orders per specification lifts the corpus mean to 0.778.

## The structure behind it

The kept sets show the mechanism, and it is coarse. `detector` reaches exactly two maximal sets, `{0}` and `{1,2,3,4,5,6}`: part 0 conflicts with all six others, so the walk keeps 1 part when it admits part 0 first and 6 otherwise. Specification order admits part 0 first by construction, and 27 of the 200 shuffles (13.5%, against the 1/7 = 14.3% chance of drawing part 0 first) do the same. `simple-arbiter` has the same shape, `{0,4,5,6}` under specification order against `{1,2,3,4,5,6}`, with part 0 blocking parts 1 to 3. `full-arbiter` trades part 6 against parts 7, 8 and 9. `lift` is the diffuse case, with 14 distinct maximal sets spread over 4 score levels.

Specification order sits at an extreme of the distribution on seven of the nine. It ties the worst shuffle on `detector`, `simple-arbiter`, `takeoff-tlsf` and `full-arbiter`, and it is the best any order reached on `arbiter-aurus`, `load-balancer` and `arbiter-handshake`.

## What a shuffled walk costs

Marginal `ltlsynt` execs per extra shuffled walk, measured with the cache shared across all 201 walks of one specification and read against a cold walk's `n_parts` execs, run: `full-arbiter` 11.5 of 16 (72%), `humanoid-531` 11.5 of 16 (72%), `lift` 11.9 of 16 (75%), `humanoid-458` 5.0 of 11 (46%), `prioritized-arbiter` 4.5 of 11 (41%), `gyro-var1` 2.2 of 9 (24%), `load-balancer` 1.1 of 8 (14%) and `detector` 0.6 of 7 (8%). The larger the part count, the less a shuffled walk reuses.

This bears on the cost argument in `STATUS-GRADING.md`, which measured 60 arbiter mutants issuing 300 MRS queries that resolved to 112 `ltlsynt` execs, 2.33x the tiered score's execs rather than the 5x the guarantee count suggests, "because greedy prefixes recur across near-identical candidates". Randomising the order per candidate is what would destroy that recurrence, and the 72-75% figures on the 16-part specifications measure how little sharing survives a shuffle.

## amba

`amba` has 52 parts and is unrealisable, so no order can keep all 52 and 51 is the highest score any admission order reaches. Specification order reaches it under both budgets tried, keeping 51 for a score of 0.981 on a walk that times out nowhere. No reordering improves `amba`, whatever the budget. A shuffle can only lose parts, and that loss is the quantity the timeouts confound.

The primary run gives each order a 120s per-call budget over 40 seeded shuffles rather than 200, one walk being 52 near-full-specification synthesis queries. Twelve of the 40 match specification order at 51 kept parts, eleven keep 50, nine keep 49 and two keep 48, and the remaining six keep 47, 46, 45, 44, 43 and 43. That inverts the corpus trend, where 19.3% of shuffled walks score above specification order. The headroom upward is zero by construction, against a measured downside of 8 parts. The 40 shuffles reach 28 distinct maximal sets, against the 2 that `detector` and `simple-arbiter` reach and the 14 of `lift`, so `amba` is the corpus's diffuse case by a wide margin.

Both runs are lower bounds rather than measurements of admission order. The 30s run hit 139 timeouts across 2114 `ltlsynt` execs, a rate of 6.6%, and bottomed out at 38 kept parts with two walks matching specification order; the 120s run hits 89 across 2102, a rate of 4.2%, and bottoms out at 43 with twelve. An undecided query resolves as unrealisable, so the part is rejected and the walk keeps fewer than the order alone would give it. Comparing the two runs order by order, 27 of the 41 orders returned a different kept count, all of them upward at the larger budget. Scores rising monotonically as the budget rises is the evidence that a large part of the apparent loss was undecided queries. Neither distribution is settled, and the tighter one cost 2879s of wall time against 791s.

The cost trend of the previous section ends here. Across the 41 walks of the 120s run the cache returned 30 hits against 2102 execs, so a shuffled walk still costs essentially a full cold walk's 52 execs. Almost no sharing survives. The timeout rates add that random-order subsets are harder synthesis queries in their own right, beyond being less cacheable: the rate stays elevated at four times the budget, while specification order, which walks contiguous blocks of a TLSF section, times out nowhere at either budget.

## What the measurement implies

The determinism the header cites is recoverable without specification order. The fitness function holds no RNG in scope — the type is `std::function<double(const Spec&)>` and the lambda at `src/fitness/function.cpp:40` captures only the grading mode — so threading a `RandomSource` through would mean changing that signature. Deriving the permutation from a hash of the candidate's own lowered formula together with the run seed needs none of that, and gives one candidate one score within a run and across reruns of that seed.

Doing so would still cost a stated property. `STATUS-GRADING.md` claims the score is monotone under guarantee-set inclusion, and a candidate-derived permutation changes with the candidate, so adding a guarantee can reorder the walk and lower the score. Removing the bias and keeping the monotonicity are incompatible.

Where the aim is a better gradient instead of an unbiased one, the data points at best-of-k orders: the corpus mean rises from 0.702 to 0.778 taking the best of 200, which is a maximum-seeking variant of the same walk, at k times the query cost and with monotonicity intact. The measurement here covers 21 unmutated corpus inputs, and the population a genetic algorithm (GA) scores is mutants of those. Whether the same order sensitivity holds along search trajectories is untested, which is the caveat `STATUS-GRADING.md` states about its own assumption chains.

Order is worth 0.045 on the corpus mean, and most of that comes from three specifications where one early part blocks the rest of the guarantee side. The walk's bias is a property of those few conflict structures rather than of greedy admission in general.
