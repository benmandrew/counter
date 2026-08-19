genetic/accumulator.hpp
=======================

Repairs found part-way through the search, kept so that a run's output is not restricted to whatever its final population happens to hold. A run reports the maximal antichain of the last generation, so a candidate that passed the output gate in generation 3 and was not selected into generation 4 is a repair the search found and then discarded. Repair quality is judged existentially over the emitted set, so keeping such a candidate can only add to it.

Off unless ``genetic.accumulate_repairs`` is set; see :doc:`../../configuration`.

``RepairAccumulator``
  Collects gate-passing specifications across generations, deduplicated and uncapped, in the order they were first seen. A disabled instance drops every insertion, so a caller need not branch on the key. It only ever reads, and never draws from the ``RandomSource``, so the seed stream is the same whichever way the key is set.

``AccumulatedRepairWriter``
  Writes each newly accumulated specification into ``<output-dir>/accumulated/`` the moment it is accumulated, one file per specification, closed on the spot so a killed run keeps what it had already found. It writes through the path's own serialiser — ``to_json`` or ``tlsf::write`` — so an accumulated file is a specification document with its tombstoned guarantees omitted. These are raw gate-passing candidates; ``repair_N.json`` / ``repair_N.tlsf`` remain the run's only filtered output.

``merge_accumulated``
  Appends the accumulated specifications a collection does not already hold, returning how many were added. The accumulated members already passed the gate, so they are merged rather than re-checked.

``AccumulatorStats``
  Process-wide record of what the accumulator contributed, reported under ``--diagnostics`` and as ``n_accumulated_repairs`` in ``run.json``.

.. doxygenfile:: accumulator.hpp
