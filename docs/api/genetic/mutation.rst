genetic/mutation.hpp
=====================

Mutation operators for genetic search over the space of FRETISH requirements.
``mutate_formula`` rewrites the formula using ``Formula::rewrite_post_order``,
replacing atoms with randomly chosen alternatives from the atom pool and
randomly inserting or removing logical connectives. ``mutate_timing`` replaces
the timing variant or adjusts tick counts. ``mutate_requirement`` applies each
of these independently at the probabilities in ``Config``.
``mutate_specification`` selects one requirement at random and replaces it with
a mutated version.

.. doxygenfile:: genetic/mutation.hpp
