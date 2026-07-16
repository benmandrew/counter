Architecture
============

Counter is a C++17 tool that repairs unrealisable `FRET <https://nasa.github.io/fret/>`_
specifications using a genetic algorithm. An input specification — a set of
assumptions and guarantees expressed in FRETISH — is evolved over several
generations until realizable variants are found. Repair candidates are ranked
by semantic and syntactic proximity to the original.

Algorithm flow
--------------

1. **Load** a :class:`Specification` from ``--input`` JSON. Each requirement
   carries a condition formula, a response formula, a :type:`Timing` modifier,
   and a derived LTL string.

2. **Build fitness** — an :class:`AggregateWeightedFitnessFunction` combining
   four weighted components: syntactic similarity, semantic similarity,
   Halstead complexity, and realizability status. A per-generation
   :class:`FilterFunction` list (false-trigger pruning, deduplication, optional
   weakening) is also constructed from the original spec.

3. **Seed** an RNG (from ``--seed`` or ``std::random_device``); register crash
   metadata so any signal handler can record the seed in the crash log.

4. **Evolve** for ``Config::generations`` rounds via ``evolve_generation``:

   a. Sort the population by fitness (descending); take the top
      ``Config::population_size`` as parents.
   b. Apply crossover (swap condition/response/timing fields between parent
      pairs) then mutation (rewrite a random sub-formula or timing modifier) to
      produce offspring.
   c. Score the offspring in a thread pool via
      :class:`AggregateWeightedFitnessFunction`.
   d. Apply the per-generation filters: false-trigger removal, deduplication,
      optional weakening (keep only specs implied by the original).

5. **Collect** the realizable survivors from the final population, re-checked
   with ``black`` + ``ltlsynt``.

6. **Apply final filters**: deduplication, then the optional implication filter
   to keep only maximal specs under the implication partial order.

7. **Score, sort, and write** each surviving spec to
   ``<output-dir>/repair_N.json``.

Key types
---------

``Timing``
  ``std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks,
  AfterTicks, Eventually, Always>``.  Encodes FRETISH temporal operators.  Defined in
  ``requirement.hpp``; constructors live in the ``timing::`` namespace so
  ADL finds them for variant visitors.

``ConditionType``
  ``enum class { Trigger, Continual }``.  Controls whether a
  :class:`Requirement` fires on a rising edge of its condition (``Trigger``) or
  at every timepoint where the condition holds (``Continual``).

``Requirement``
  The unit of repair.  Holds ``m_condition``, ``m_response``, ``m_timing``,
  ``m_condition_type``, and the derived ``m_ltl`` string produced by
  ``requirement_to_ltl``.

``Specification``
  A list of assumption :class:`Requirement` objects, a list of guarantee
  :class:`Requirement` objects, and the input/output atom universes used by
  crossover and mutation.

``Formula``
  A propositional AST (PImpl) supporting parse, simplify, DIMACS conversion,
  syntactic similarity, and post-order rewriting.  See ``prop_formula.hpp``.

``TransferSystem``
  A finite-state automaton (states, valuation-count vector, transition matrix)
  built from an LTL formula via ``ltl2tgba``/Ganak.  Used by the semantic
  similarity and model-counting subsystems to count satisfying bounded traces
  via matrix exponentiation.

``Count``
  ``long double``.  Trace counts are only consumed as ratios cast to
  ``double``, so exponent range matters more than exact integer width.  All
  arithmetic must go through ``count_add_overflow`` / ``count_mul_overflow``
  with an assert on the overflow flag; overflow means the result went
  non-finite.

Module layout
-------------

.. code-block:: text

   include/
     bounded_async.hpp   — bounded-concurrency async dispatch
     config.hpp          — compile-time algorithm parameters
     prop_formula.hpp    — propositional formula AST (Formula)
     requirement.hpp     — Timing, ConditionType, Requirement, Specification
     serialisation.hpp   — JSON serialisation for all core types
     thread_pool.hpp     — fixed-size worker thread pool
     crash/              — crash_handler: SIGSEGV/SIGABRT/SIGFPE reporter
     filter/             — bloat cap, weakening, implication, dedup filters
     fitness/            — syntactic/semantic similarity, Halstead, status, model counter
     genetic/            — mutation, crossover, generation evolution, random source
     runner/             — thin wrappers for black, ganak, ltlsynt/ltl2tgba (SPOT)

External tools
--------------

``ltl2tgba``, ``ltlsynt``
  From the SPOT library.  Built from source via ``cmake/spot.cmake``; located
  via the ``SPOT_BIN_DIR`` compile macro.  Used for automaton construction and
  LTL realizability checking.

``black``
  LTL satisfiability checker (``black-sat``).  Found on ``PATH`` or downloaded
  via ``cmake/black.cmake``; path passed as ``BLACK_EXECUTABLE_PATH``.  Used
  for the status fitness component and implication-based filters.

``ganak``
  Weighted model counter.  Downloaded as a release binary via
  ``cmake/ganak.cmake``; path passed as ``GANAK_EXECUTABLE_PATH``.  Used to
  count satisfying valuations for each automaton transition, enabling
  trace-level model counting via the ``TransferSystem`` matrix.
