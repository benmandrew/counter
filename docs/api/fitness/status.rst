fitness/status.hpp
==================

``status_score`` scores a candidate on a three-point scale by running ``black``
on each of its component formulae and, once all of them hold, ``ltlsynt`` on
the whole specification:

.. list-table::
   :header-rows: 1

   * - Score
     - Meaning
   * - 1.0
     - Realisable
   * - 0.5
     - Every component satisfiable, but the specification is unrealisable
   * - 0.0
     - Some component formula is unsatisfiable on its own

Both front ends score on this scale, and differ only in how a candidate
decomposes into components and in how realisability is queried.
``specification_status`` uses the per-requirement conjunction
``condition & response``; ``tlsf_status`` uses the individual formulae of the
six TLSF sections.

The scale was previously five-point, grading the region below realisability by
whether the guarantee side or the assumption side was jointly unsatisfiable.
Instrumenting both front ends over 887k scored candidates put under 0.32% of
the population into those tiers, and the assumption-side tier never fired at
all, so both were removed. Ranking *within* unrealisability needs a measure of
how far a candidate is from realisable, which a satisfiability query cannot
express.

Weighted equally with the semantic similarity component so realisable
candidates are strongly preferred throughout evolution.

.. doxygenfile:: status.hpp
