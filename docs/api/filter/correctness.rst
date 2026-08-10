filter/correctness.hpp
======================

Holds the correctness properties a written repair must satisfy, as one ordered table per front end: ``correctness_checks`` here, and ``tlsf_correctness_checks`` in ``tlsf/filter.hpp``. Each row carries a display name, an admissibility predicate, and the ``Config`` flag that turns that property's per-generation stage on. The two tables name the same properties in the same order, so the paths cannot drift apart the way two hand-mirrored lists did.

Three consumers read a row. The per-generation filter chain builds a stage from it when its flag is set, so the flags decide search pressure alone. The gate that collects the realizable survivors applies the predicate to every one of them, whatever the flags say, which is what turns a property into a claim about the output rather than about the search. The input screen applies the same predicates once to the specification the run starts from, and warns rather than rejects. A property enforced only in the per-generation chain leaks through everything the search did not breed --- an elite, which bypasses the offspring filters by design, and the whole seed population, which no filter sees at all.

The rows run cheapest first, and ``not-well-separated`` runs last. ``vacuity`` leads because its syntactic screen costs no solver call and its ``black`` queries are keyed per requirement, so a candidate bred from a scored parent pays only for what mutation changed. Well-separation is the one check that can be *cold* at the gate: realizability is a scored fitness objective, so the final population's verdicts are already memoised, and vacuity's per-requirement keys are warm for the same reason, but nothing in a run warms an ``ltlsynt`` well-separation query when its per-generation stage is off. Putting it last means a candidate rejected on a warm query never reaches the cold one.

.. doxygenfile:: correctness.hpp
