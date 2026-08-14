genetic/crossover.hpp
======================

Crossover operators that combine two parent requirements or specifications to produce an offspring, following AuRUS. ``crossover_requirements`` *grafts*: for the condition and again for the response, it replaces a subformula of the first parent's field with one drawn from the second parent's, or joins the two under a fresh binary operator, each with probability 1/2. No branch copies a field verbatim, so every crossover recombines. The timing is crossed separately, AuRUS having no analogue for it.

``crossover_specifications`` merges one requirement per side. For the assumptions and again for the guarantees, it draws one slot of the first parent and one of the second, uniformly and independently, and writes their crossover into the first parent's slot. The donor need not occupy the target slot, which is what lets a subformula of guarantee 3 reach guarantee 0. Every other slot is inherited from the first parent, whose shape the offspring therefore keeps whatever the second parent's is; only the in/out atom universes have to match. Deleted and non-weakenable requirements take no part on either side.

.. doxygenfile:: genetic/crossover.hpp
