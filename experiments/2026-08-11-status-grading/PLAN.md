# Status grading scale: `tiered` against `mrs`

Pre-registered 2026-08-11, before any row at the campaign commit was collected. No accept/reject rule is registered; §5 sets out why.

## 1. Question

`[fitness] status_grading` selects the scale the `status` fitness objective grades on. Two values exist, and this campaign runs one arm per value.

`tiered` is the shipped default and the baseline arm. It grades on three points: 0.0 for a component unsatisfiable on its own, 0.5 for satisfiable but unrealizable, and 1.0 for realizable and well-separated.

`mrs` grades on the greedy *maximal realizable subset* (MRS) fraction. The lowered guarantee side is split into top-level conjuncts and walked in specification order, keeping each part whose addition leaves the accumulated subset realizable against the full, unchanged environment side. The score is the number kept over the number of parts, and 1.0 still requires well-separation.

The question is whether the finer scale changes what the search finds.

## 2. Why the question is open

Three levels give a *genetic algorithm* almost no gradient to follow. Over the 21 specifications in `examples/`, the tiered scale scores every one of them 0.5, so the objective is a constant across the entire corpus and contributes nothing to ranking. MRS spreads the same 21 specifications over 14 distinct values, with a median of 6 grade levels per specification and a maximum of 17.

Existence of a gradient and reachability of it are separate properties. The pilot measurements behind those figures were taken on assumption-strengthening chains, and that is not a trajectory the search walks. Whether crossover and mutation reach the extra grade levels, in the order they would have to be reached, is what this campaign measures.

## 3. Design

One factor, sweep G, at two levels: `tiered` and `mrs`. It is expressed as a sweep rather than as a crossed factor directory so that the arm rides in `level_name`, a field the merge key already carries.

| | `status-grading` |
|---|---|
| corpus | the 20 specifications in `TLSF_ABLATION_SPECS` |
| operating point | gen 10 / pop 200 |
| arms | `tiered`, `mrs` (sweep G) |
| selection scheme | `nsga2-truncate` |
| metric | `direct` |
| seeds | 24 (av2 0–11, av3 12–23) |
| runs | 20 × 2 × 24 = 960 |

Everything other than the sweep key sits at the campaign defaults. Selection is `nsga2-truncate`, the shipped scheme and the one the status objective is ranked under in production.

Both binaries are built from the branch `feat/mrs-status`. `campaign.py stage` verifies that each host's binary reports the declared commit with `dirty=0`, so a stale or locally modified build fails staging rather than contributing rows that look valid.

Seeds are disjoint across the two hosts, so a host lost mid-campaign still leaves a balanced design over every specification and both arms.

## 4. Endpoints

Fixed before launch.

**Primary.** Paired yield: `n_repairs` per `(spec, seed)`, `tiered` against `mrs`.

**Secondary.** `implies_ideal`, the repair-quality measure scored against the ideal repairs in `examples/*/fixes`.

**Tertiary.** Wall time per run, which is the cost of the grading.

Pairing is within `(spec, seed)`, and both arms of a pair run on the same host, so host differences cancel inside a pair.

## 5. No decision rule

No accept/reject rule is registered. The endpoints in §4 are fixed in advance, and the decision on whether MRS becomes the default is deferred to inspection of the results.

This is deliberate. A threshold registered now would be a guess at an effect size nobody has measured on a trajectory the search walks, and §6 gives three reasons the obvious thresholds would be read wrongly. Fixing the endpoints without a rule keeps the record of which comparisons were planned, and leaves the defaults question to a campaign that can be sized against a measured effect.

## 6. Threats to validity

Recorded before the fact, so none of them is discovered afterwards in the shape of whichever result arrives.

**Yield is high-variance across seeds.** In the pilot, `amba` yielded 0 repairs at seed 0, 0 at seed 1, and 7 at seed 2. A campaign reading yield alone at low seed counts would be badly misled by that spread.

**The pilot is directional only.** A 3-seed pilot over the same 20 specifications gave 33 complete pairs: MRS ahead on 8, behind on 3, tied on 22, sign test p = 0.227. The 22 ties are the binding constraint, since power on a paired sign test comes from the discordant pairs alone.

**Cost varies by more than an order of magnitude across specifications.** On `amba`, MRS costs 4.8× the wall time and 31× the `ltlsynt` calls of tiered. On `gyro-var1` and `humanoid-458`, MRS is *faster* than tiered despite making 4–5× the calls. A per-specification timeout cap sized from one specification would censor another, so caps are set per specification.

**Censoring is not a cheap run.** Any run reaching its cap is recorded as timed out, and a censored run is no evidence about yield in either direction. Recording it as zero repairs would understate whichever arm is the more expensive one on that specification, which the spread above makes specification-dependent.

## 7. Provenance

Campaign directory `experiments/2026-08-11-status-grading/`, tracked contents: this `PLAN.md`, `PROVENANCE.json` written at launch, and `scripts/` — verbatim copies of the harness scripts that ran it, with blob shas recorded.

The pilot cost sweep behind §6's cost figures was an ad-hoc script rather than a harness profile, run at commit `9075fbb`. A later rebase made that commit unreachable; the tag `provenance/mrs-cost-sweep` holds it, so those figures stay checkable against the code that produced them.

The archived configs record only what sweep G overrides. Every other value comes from the binary's default at run time, so reproducing this campaign requires the commit `PROVENANCE.json` names, on the `feat/mrs-status` branch.

A plan with endpoints and no decision rule is an unusual shape for this directory, and it is the shape this question is in. The rule belongs in the campaign that follows, written against a measured effect rather than against an expectation of one.
