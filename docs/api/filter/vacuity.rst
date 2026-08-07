filter/vacuity.hpp
==================

Detects specifications that are *vacuously* realizable. Realizability is decided on ``(assumptions) -> (guarantees)``, so a candidate whose assumptions contradict one another is realizable regardless of its guarantees: a false antecedent makes the implication a tautology. Such a candidate repairs nothing, and the weakening filter cannot reject it, since an unsatisfiable assumption implies every other assumption and so passes every implication test.

Only assumptions are checked for satisfiability. Unsatisfiable guarantees make the implication unsatisfiable and are therefore already reported unrealizable, which the search punishes without help. The check short-circuits for specifications with no assumptions and treats a solver timeout as satisfiable, so a slow check never silently discards a candidate.

The guarantee side is checked too, for the dual failure: a guarantee that is *valid*, and so demands nothing. Each guarantee is negated and tested individually, and the specification is rejected on the first one whose negation is unsatisfiable. The weakening screen is least able to catch this, since the original implies a valid guarantee trivially and a gutted guarantee therefore reads as a perfect weakening.

Per guarantee rather than over the guarantee conjunction, and the two are not interchangeable: the conjunction is valid only when *every* guarantee is, so it would keep a candidate that guts one conjunct into a no-op while the rest still constrain the system — which is exactly what mutation reaches. It is also the cheaper query, against a cache keyed per requirement, so a guarantee carried over from a parent costs nothing after its first evaluation. The assumption side takes no such split: that check is joint *satisfiability*, which does not distribute, since ``G p`` and ``G !p`` are each satisfiable and jointly are not.

A syntactic screen runs ahead of both, under the same flag and the same ``vacuity`` stage: a condition that is the literal ``false``, checked in assumptions and guarantees alike. The condition sits only in the antecedent of the lowered implication, under every timing and both condition types, so this one is vacuous unconditionally. It is the assumption side that needs the screen, a vacuously-true assumption being satisfiable and so invisible to everything semantic here.

There is deliberately no syntactic dual over responses. A ``true`` response is a no-op under most timings but *not* under ``after n ticks``, whose lowering negates the response — making ``true`` there the strongest guarantee expressible rather than an empty one. Only the semantic check reads the lowered formula, so only it gets that case right, and it does so without a ``black`` call: ``ltlfilt`` constant-folds these negations before the solver is reached.

Every solver verdict is conservative in the same direction. A timed-out guarantee query reads as falsifiable, exactly as a timed-out assumption query reads as satisfiable, so no candidate is ever dropped on a non-answer.

.. doxygenfile:: vacuity.hpp
