fitness/status.hpp
==================

``specification_status`` scores a candidate on a five-point scale by running
``black`` and ``ltlsynt`` on the conjunction of its LTL formulae:

.. list-table::
   :header-rows: 1

   * - Score
     - Meaning
   * - 1.0
     - Satisfiable and realisable
   * - 0.5
     - Satisfiable but unrealisable
   * - 0.2
     - Conjunction unsatisfiable; assumptions and guarantees individually satisfiable
   * - 0.1
     - Trigger conjunction satisfiable; response conjunction not
   * - 0.0
     - Trigger conjunction unsatisfiable

Weighted equally with the semantic similarity component so realisable
candidates are strongly preferred throughout evolution.

.. doxygenfile:: status.hpp
