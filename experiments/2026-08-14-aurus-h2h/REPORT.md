# Mutation and crossover defects behind the aurus-h2h quality gap

Written 2026-08-19, after the campaign closed. `PROVENANCE.json` records the decision and the mechanism the campaign itself could establish: AuRUS scores higher on the pre-registered repair-quality endpoint, and it returns a median of 317 candidates per repeat against counter's 4. This report covers what the same rows say about counter's genetic operators, a claim about the engine rather than about the campaign's factors. Nothing here revises the decision.

counter's mutation grammar cannot express the shape of a minimal guarantee weakening. Its cheap moves gut a specification, and its precise moves need three chained draws through unrealizable intermediates, so the repairs it returns land strictly weaker than the scoring targets or off the implication order entirely. Sections 1 to 5 give the rows and the source lines behind that.

## Method

The analysis draws on three sources, in decreasing order of weight.

The archived corpus is 2,295 repair files across 500 run directories, over 25 families and 20 seeds, written by the campaign's counter arm at commit `1d18cf3` under the shipped defaults. Population-level counts come from the `Realizable specifications: N (M maximal)` line, which 480 of the 500 `run.log`s carry. The diversity sweep recorded in `PROVENANCE.json` swept a slightly different directory set and reports 2,251 files, so the two counts describe overlapping rather than identical scopes. Section contents were parsed directly out of the `.tlsf` files. The assumption side only ever grows, so an `ASSUME` entry past the original's count is an appended assumption and everything at or below that index is a mutated original. The two are separable by slot position, and conflating them inflates the appended count by a factor of four.

Twelve local runs at HEAD (`d37ce0e`, clean) with `--dashboard` supply the per-generation diversity series, which the campaign did not record because `dashboard` defaults off.

A generation-prefix replay supplies the counterfactual. counter is deterministic given a seed, so a run at `generations = k` reproduces exactly the population the `generations = 10` run held at generation *k*. The union over k = 1…10 is a cross-generation archive simulated with no code change. Determinism was checked directly: every k = 10 run reproduces its dashboard run's realizable and maximal counts exactly.

## 1. In-place growth is polarity-gated

`mutate_atom_formula` (`src/genetic/mutation.cpp:55`) offers two moves on an atom: rename it, or wrap it in `Not`. No rule expands an atom into a binary node. The whole propositional core holds one in-place growth rule, `mutate_not_subtree`'s third case (`src/genetic/mutation.cpp:70`), which builds `anchor o2' ¬child`. It fires only at a `Not` node.

A weakening that guards a negated literal therefore costs one draw, and the same weakening on a positive literal costs three — rename, negate, then graft. Both intermediates are ordinary members of the population and must survive selection to reach the third step.

`gyro-var2` shows the rule working. Its repair weakens

```
original:  G F ( balancer_0 && !balancer_1                  && !balancer_2 )
repair:    G F ( balancer_0 && (backdistsense || !balancer_1) && !balancer_2 )
```

which is the graft firing at `!balancer_1` in one step. On `minepump` the same rule is blocked. Its guarantee `G(high_water -> X pump_on)` needs the antecedent to become `high_water && !methane`; `high_water` is positive, so no single draw touches it.

## 2. `Implies` is absent from the temporal connective pool

`pick_binary_kind` on the *Temporal Logic Synthesis Format* (TLSF) path draws from {∧, ∨, U, R, W} (`src/tlsf/mutation.cpp:37`), and the comment records the exclusion of `Implies` and `Iff` as deliberate, following Brizzio's fragment. Every case of `mutate_temporal` at a binary node either collapses to one child or re-emits under a freshly drawn connective (`src/tlsf/mutation.cpp:156`). A temporal mutation reaching an implication therefore always destroys it.

AuRUS mutates Owl's *negation normal form*, in which `a -> b` is stored as `¬a ∨ b` and no `Implies` node exists to exclude. Its `visit(Disjunction)` then carries an add-disjunct rule (`GeneralFormulaMutator.java:455`) that covers exactly the case counter's exclusion closes off. counter kept `Implies` as a first-class node and inherited the exclusion without the compensating rule, so implications in a specification are reachable only to be broken. The exclusion is sound in the representation it came from.

`mutate_propositional_parts` (`src/tlsf/mutation.cpp:237`) reconstructs every temporal node verbatim and recurses, preserving both the implication and the temporal skeleton, so an `F` never becomes an `X`. `mutate_temporal` introduces the `X` and takes the implication with it. Neither path performs the edit a guarded implication needs.

## 3. The assumption template hard-wires `F` and single-literal leaves

`tlsf_add_assumption` (`src/tlsf/mutation.cpp:291`) emits `G F a`, or `G(b -> F a)` at `p_conditional_assumption`, over single drawn literals. Appended assumptions are then mutable like any other conjunct. The corpus records how far that gets them.

| appended assumptions across 2,295 repair files | count | share |
|---|---|---|
| total, appearing in 274 files | 783 | 11.9% of files |
| still verbatim template | 646 | 82.5% |
| template skeleton kept, mutated inside | 32 | 4.1% |
| skeleton changed | 105 | 13.4% |
| still containing an `F` | 742 | 94.8% |
| ever acquiring an `X` | 55 | 7.0% |
| reaching `… -> X(…)` | 3 files, 1 distinct formula | 0.4% |

That one formula is `G(!b2 -> X b2)`, on `lift` seed 19. It is the corpus's only instance of the shape `rg1`'s ideal needs, and its antecedent is negated, which is defect 1 again. Every mutated appended assumption in the corpus grew around a `Not`.

```
G(F( a1 -> !r1 ))          lily15
G( (b3 -> f2) -> F b2 )    lift
G( !b2 -> X b2 )           lift seed 19
```

The template negates its drawn atom on a coin flip. Half of all appended assumptions therefore carry a `Not` for the graft rule to bite on, and half are frozen as bare literals for the life of the run.

## 4. Three operators reach the top status tier in one draw

`minepump` is unrealizable because `high_water && methane` forces `X pump_on` and `X !pump_on` together. Its three ideals guard: `fixed0` adds the assumption `G(!high_water || !methane)`, `fixed1` and `fixed2` add one literal to a guarantee's antecedent. Every repair counter returns instead removes a guarantee outright:

| repair | guarantee side |
|---|---|
| 0, 4 | `G(methane -> X !pump_on)` twice, slot 1 overwritten by slot 2 |
| 1 | `G(high_water -> X pump_on)` alone |
| 2 | `G(methane -> X !pump_on)` alone |
| 3 | `G(high_water -> X pump_on)` twice |
| 5 | guarantee 2 replaced by `X(F !methane \|\| XX F !pump_on)` |

Three separate operators produce that outcome: `tlsf_remove_guarantee` (`src/tlsf/mutation.cpp:336`), crossover writing a donor conjunct into a slot so a side holds the same formula twice, and `mutate_temporal` scrambling a guarantee into something trivially satisfiable. Three seeds re-run at `p_remove_guarantee = 0.0` returned 4, 1 and 4 repairs, all still strictly weaker than the ideals and none implying one, so blocking one operator changes nothing.

The returned candidates score `(syntactic 0.727, semantic 0.779, status 1.0)` and `(0.667, 0.0, 1.0)`. A `fixed1`-shaped repair would dominate both on every objective, having added one literal rather than destroyed a conjunct. Selection is therefore not what keeps the guarded repair out. Gutting reaches status 1.0 in a single draw, and the path to the guard passes through two unrealizable intermediates that score below it.

AuRUS reaches `fixed1` in one draw at roughly one in eight. `visit(Disjunction)` picks its add-disjunct branch one time in four, and `new_literal` (`GeneralFormulaMutator.java:763`) retries for a variable absent from the formula, here `methane`, then negates it on a coin flip. It scored 30/30 repeats on this family.

## 5. Further defects the same corpus evidences

An audit of the operator sources against the same 2,295 repairs found seven more. The counts below were re-measured independently of the audit except where noted; where the two scans disagreed, the disagreement is stated rather than resolved in favour of the tidier number.

**An assumption that constrains an output escapes the safeguard the code names for it.** `tlsf_add_assumption` draws both the guard and the body from inputs ∪ outputs under `allow_output_assumptions` (`src/tlsf/mutation.cpp:307`), and the comment above it records well-separation as what stops the system writing itself an assumption it can force to fail. The corpus splits sharply on shape. Of 783 appended assumptions, 302 are conditional, and *180 of those have an output as the body*, appearing in 175 repairs, while *zero* unconditional `G F <output>` survive anywhere. Examples are `G(r2 -> F !g2)` on `arbiter-aurus` and `G(request_1 -> F !grant_0)` on `load-balancer-aurus`. An environment that never raises the guard satisfies such an assumption, so the checker calls it well-separated by the definition it implements. Well-separation is weaker than "the system cannot defeat this assumption", and the conditional form sits in that gap. 19 of the 180 have outputs on both sides.

**The conditional form can also be a tautology.** The guard and the body are drawn independently at the same site (`src/tlsf/mutation.cpp:318`), with nothing forbidding the same literal twice, so `G(l -> F l)` is emitted — 30 of 783 appended assumptions, including `G(balancer_1 -> F balancer_1)` on `gyro-var1` and `G(!r_1 -> F !r_1)` on `simple-arbiter-aurus`. Each one consumes the operator's budget and makes "an assumption was added" a misleading count.

**Every propositional leaf is rewritten, and a one-atom subtree is rewritten with certainty.** `mutate_formula` gates each node on `next_index(n_subformulas) != 0` (`src/genetic/mutation.cpp:112`), and `mutate_propositional_parts` calls it once per maximal propositional subtree (`src/tlsf/mutation.cpp:237`). For a single-atom subtree `n_subformulas` is 1, so the draw is always 0 and the atom always changes. A conjunct such as `G(a -> F b)` therefore has both leaves replaced on every call. The same gate fails at the other end: for one block of *n* nodes the chance that no node mutates is (1 − 1/n)ⁿ, approaching 37%. Four mutations in five take this path, so the propositional neighbourhood is coarse in one direction and empty in the other.

**`mutate_temporal` has no leave-alone branch.** Every arm of the switch rewrites its node and recurses into both children (`src/tlsf/mutation.cpp:103`), and the atom arm forces a *distinct* atom whenever the pool allows (`src/tlsf/mutation.cpp:87`). Measured over conjuncts that can be aligned with their original by position, the audit found 3,279 of 3,503 changed conjuncts (93.6%) differ in *abstract syntax tree* (AST) shape, with a size ratio reaching 14.67×. One mutation regenerates the conjunct rather than perturbing it, which is why the temporal path degrades a guarantee instead of exploring around it.

**Nothing simplifies a temporal node.** `simplify_node` returns `nullopt` for every temporal kind (`src/prop_formula/simplify.cpp:176`), while the propositional folder collapses `lhs == rhs` and constants. Everything the operators build at a temporal node therefore survives to output: 102 occurrences of the nesting `G(G(` and 99 of `F(F(` across the corpus, against zero in any of the 25 originals, plus self-joins under `W` and `U` that the `∧`/`∨` folder would have caught. `lift` seed 2 writes an INITIALLY of `φ W φ`, and `lift` seed 17 writes `(true) W (false)` and `(false) W (…)`. Constants are not manufactured, 12 of the 25 originals containing one already, but nothing removes them once mutation has flipped and combined them.

**Atom pools are per-side rather than per-section.** One pool serves all three sections on a side (`src/tlsf/mutation.cpp:399`), and crossover lets an ASSUME donor land in an INITIALLY slot. Against families whose originals contain none, the corpus holds 15 INITIALLY entries carrying a temporal operator and 8 PRESET entries carrying an input — for instance a `pcar-v2-888` INITIALLY containing `X(!obstacle)`. Basic TLSF requires the initial conditions to be propositional and over the right signal set, so these repairs are outside the format they are written in, independently of whether they repair anything.

**The bloat cap's baseline is the largest formula anywhere in the original.** `tlsf_make_bloat_cap_filter` caps every conjunct at `max_ratio` times `max_formula_size(original)` (`src/tlsf/filter.cpp:410`), so a specification with one large conjunct and several small ones leaves the small ones effectively uncapped. `gyro-var1` seed 17 grew a 6-node REQUIRE to 47 nodes and `lift` seed 17 grew a 1-node ASSUME to 10, both under the cap. Counting how many conjuncts exceed twice the largest original of *their own section* gives 304 in 278 repairs by the audit's AST measure and 424 in 354 by a token-based one; the two metrics disagree on the number and agree on the direction.

Two further defects are on the FRETISH path and cannot be measured here, this corpus being TLSF-only. Crossover's graft site is chosen by a fair coin at each node of a post-order walk (`src/genetic/crossover.cpp:83`), so the k-th node is reached with probability 2⁻ᵏ and no graft happens at all with probability 2⁻ⁿ. For a single-atom condition that is half the time, and the offspring field is then the first parent's. The TLSF counterpart draws uniformly (`src/tlsf/crossover.cpp:97`), so the two paths differ while both are documented as AuRUS's `replaceSubformula`. Separately, `mutate_atom_name` on the FRETISH path has no distinctness guard where the TLSF one does (`src/genetic/mutation.cpp:52` against `src/tlsf/mutation.cpp:87`), making a rename a no-op one time in the pool size.

Seven leads were checked and dismissed. No repair contains a double negation, both folders removing it. `X(X φ)` is not redundant and 14 originals already contain it. Every atom in every repair is a declared signal. No unconditional self-serving assumption survives, which is what makes the conditional leak above sharp rather than diffuse. Repairs do not grow monotonically, a quarter of them being strictly smaller than their specification. `allow_output_assumptions` behaves exactly as documented. *Tombstoned* slots are excluded correctly on both the mutation and crossover sides, and the live-guarantee floor makes the empty-side fallback unreachable.

## What this costs

`best_relation` separates the two arms by kind. Over counter's 499 runs it reads 238 incomparable, 77 strictly stronger, 63 equivalent, 63 strictly weaker and 52 none; over AuRUS's 780 it reads 264 equivalent, 149 strictly weaker, 114 strictly stronger, 20 incomparable, 197 none and 36 unknown. Nearly half of counter's runs land off the implication order altogether, where one AuRUS run in forty does. counter finds repairs at 99–100% yield on the five families where it has never implied an ideal in eleven campaigns, so the deficit is a repair of the wrong kind rather than no repair at all.

The three blocks are each visible in a family that reads zero. `rg1`'s ideals add `G(!valid_signal_high -> X !cancel_signal_high)`, the shape reached once in 2,295 files. `gyro-var2`'s adds a five-way disjunction over balancer states and `F` terms, which needs repeated grafts at atoms. `minepump`'s `fixed0` needs the `F` gone entirely, and only the temporal path removes it, which takes the implication with it.

## What this is not

Two explanations were tested against the same data and do not hold. A third holds in part, and the correction is recorded here rather than removed.

The output policy does not explain it. counter's final population holds a median of 46 realizable specifications and writes 4, discarding 89.2% of 21,215 across the arm. That discard is hit-preserving: `check_pair` drops a specification only when another strictly implies it (`src/tlsf/filter.cpp:128`), and scoring asks whether a repair implies an ideal, so transitivity carries any hit to the retained element.

The loss of earlier generations explains part of it, and an earlier draft of this report said otherwise on evidence too thin to carry the claim. The generation-prefix replay recovers 1.3–6× more repairs over five families and 11 runs and adds zero hits, every recovered repair being strictly weaker or incomparable. Campaign `2026-08-19-accumulator` then measured the same question properly, over 100 paired runs across all 25 families with `[genetic] accumulate_repairs` as the factor: exact McNemar gives 6 discordant pairs to the accumulating arm and 0 against, p = 0.0312, at a median paired wall ratio of 1.034. The replay is consistent with that rather than contrary to it — an effect of six runs in a hundred predicts well under one hit in eleven — and the family selection compounds it. The six discordant pairs are `round-robin-arbiter-aurus` seeds 1 to 3, `detector-aurus` seeds 2 and 3, and `simple-arbiter-aurus` seed 1, none of which the replay covered. All three are families whose only ideal deletes a guarantee, which is the weakest target on the lattice and therefore the one an extra candidate is likeliest to hit.

Accumulation is thus a real gain that is separate from, and much smaller than, what the operator grammar costs. It moved yield from 79 to 81 of 100 runs. It is a candidate default owing a FRETISH replication, held on `campaign/accumulator` under the tag `provenance/accumulator-campaign`, and none of the fixes below substitutes for it.

The diversity series rules out convergence. From generation 3 the population carries 100 distinct specifications out of 100 and breeds 80 fresh distinct offspring per generation, in every run measured, while best fitness is flat. The realizable stock plateaus near 50 and the maximal *antichain* stays between 2 and 19; on `gyro-var2` it is frozen at the same 2 specifications for all ten generations while the realizable stock grows eightfold.

## Fixes, ranked

**Ungate the graft.** Let the add-connective move fire at any propositional node rather than only under a `Not`, which addresses defect 1 directly and defects 2 and 3 in part. Test against `minepump`'s 20 seeds, where the prediction is that `implies_ideal` moves off 0.05, the ideal becoming a one-draw move as it is for AuRUS. That test costs about four minutes of local compute.

**Give the assumption template a non-`F` consequent.** `p_conditional_assumption` already draws a conditional shape. Drawing the consequent's modality alongside it reaches `minepump`'s `fixed0`, and it is the only route to `rg1`'s ideals that does not depend on a three-step path surviving selection.

**Re-emit `Implies` on the temporal path,** or lower implications to `¬a ∨ b` before mutating and raise them afterwards. The second is closer to what AuRUS does and needs no new rule, at the cost of a normalisation pass that the syntactic-similarity objective would then see.

**Reconsider what the gutting operators cost.** They are not defects on their own: `PLAN.md` §7.6 records that some ideals are reachable no other way, and the `drop-*` ideals need exactly this move. The measurement worth making is whether a candidate that deletes a conjunct should reach the same status tier as one that guards it, given that the guard is three draws away and the deletion is one.

Four fixes from §5 stand apart, being corrections rather than search-quality changes, and three of them are cheap.

**Draw the conditional assumption's body from inputs alone,** leaving the guard free to mention an output. That closes the 180-assumption leak without giving up the reactive shape the flag exists for, and it needs no new filter. The alternative, testing whether the system can defeat an assumption, is the stronger property but a new solver query on a path that currently has none.

**Reject a conditional assumption whose guard and body are the same literal.** One comparison at `src/tlsf/mutation.cpp:318`, removing 30 tautologies from the corpus's 783.

**Simplify at temporal nodes,** at least the idempotent nestings `G G φ`, `F F φ` and the self-joins `φ W φ` and `φ U φ`. These are pure bloat: they inflate every size-based score, consume the bloat cap, and enlarge the automata `ltl2tgba` and Ganak build.

**Make the atom pool per-section rather than per-side.** That stops an INITIALLY acquiring a temporal operator and a PRESET acquiring an input, which currently take repairs outside basic TLSF. Giving the bloat cap the same treatment looks like the matching fix and is not, for the reason the next section measures.

## What implementing this measured

Every fix above was implemented on `fix/mutation-operators` off `3cc6eba`, and the smoke test below is what the branch was checked against before anything was claimed for it. Four families, 20 seeds each, at the campaign's own configuration, on one desktop rather than the lab hosts. The control is the pre-change binary on the same box against the same ideals rather than the archived campaign rates, and it reproduces those rates exactly, which is what qualifies it as a control.

| family | control | first implementation | corrected |
|---|---|---|---|
| minepump | 1/20 | 1/20 | 3/20 |
| arbiter-aurus | 10/20 | 1/20 | 15/20 |
| rg2 | 20/20 | 18/20 | 20/20 |
| lily02 | 19/20 | 20/20 | 20/20 |

This is a smoke test rather than a campaign. Four families are not 25, the arms are unpaired because changing an operator changes the draw stream, and no decision rule was written down first. It establishes a direction and the absence of a gross regression; a paired campaign is still owed before any of this becomes a default.

Two findings came out of it that reading the corpus could not have reached.

**The per-section bloat baseline is wrong, and is dropped.** It reads as the natural companion to the per-section atom pool, and it does the opposite of what this report asks for: a weakening grows a conjunct, and a small conjunct in a section of small conjuncts has nowhere to grow into. It raised `arbiter-aurus`'s bloat-cap drop rate from 1.6% to 9.4% and cost that family 9 of its 10 ideal-implying runs; reverting it alone recovered 8 of the 9, and `rg2` returned to 20 of 20. The cap keeps its specification-wide baseline. The shape is worth naming: a filter tightening that fights an operator loosening gets attributed to the operator.

**The graft has to draw its anchor's polarity.** A first implementation drew a bare positive atom as the anchor, which cannot reach `high_water & !methane` from `high_water` — `minepump`'s ideal, and the case §1 is written around. That family stayed at 1 of 20 until the polarity draw was added, at which point it moved to 3. A rule that grows a formula but cannot negate what it grows misses half the guards there are.

Every fix here is a change to the operator set, so each needs a paired campaign before it becomes a default, and all of them together re-pinned the determinism goldens once, from 149 draws to 212. The gap the campaign measured sits in the operators rather than in the search that drives them, which is the one thing about it that can be checked one operator at a time.
