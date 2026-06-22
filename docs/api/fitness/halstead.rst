fitness/halstead.hpp
====================

Halstead complexity metrics adapted to the propositional formula domain.
Operators are logical connectives (¬, ∧, ∨, →, ↔) and timing modalities;
operands are atom names and tick counts. ``HalsteadCounts`` collects the four
raw token counts (η₁, η₂, N₁, N₂). ``halstead_volume`` computes
V = (N₁ + N₂) × log₂(η₁ + η₂).

``halstead_fitness(spec, original)`` maps the candidate's volume to a score in
[0, 1]: 1.0 when the candidate is at most as complex as the original, falling
off as volume grows beyond it. Used as a size-penalty component in the
aggregate fitness function.

.. doxygenfile:: halstead.hpp
