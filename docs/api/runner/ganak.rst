runner/ganak.hpp
=================

Wrapper around the Ganak weighted model counter. ``run_ganak_on_dimacs`` and
``run_ganak_on_formula`` invoke the Ganak binary at ``ganak_executable_path()``
on DIMACS CNF input and parse the ``Count`` result from its output.
``GanakStats`` tracks cumulative call count and wall time for diagnostics.
Called by the ``TransferSystem`` construction path to count satisfying
valuations for each automaton transition.

.. doxygenfile:: ganak.hpp
