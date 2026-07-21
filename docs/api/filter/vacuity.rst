filter/vacuity.hpp
==================

Detects specifications that are *vacuously* realizable. Realizability is
decided on ``(assumptions) -> (guarantees)``, so a candidate whose assumptions
contradict one another is realizable regardless of its guarantees: a false
antecedent makes the implication a tautology. Such a candidate repairs nothing,
and the weakening filter cannot reject it, since an unsatisfiable assumption
implies every other assumption and so passes every implication test.

Only assumptions are checked. Unsatisfiable guarantees make the implication
unsatisfiable and are therefore already reported unrealizable, which the search
punishes without help. The check short-circuits for specifications with no
assumptions and treats a solver timeout as satisfiable, so a slow check never
silently discards a candidate.

.. doxygenfile:: vacuity.hpp
