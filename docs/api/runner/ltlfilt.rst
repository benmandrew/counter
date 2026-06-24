runner/ltlfilt.hpp
==================

Normalises LTL formulae to a canonical form using SPOT's ``ltlfilt`` tool.
``normalize_ltl`` memoises results so the subprocess runs at most once per
unique input string. On binary inaccessibility or subprocess failure the
original formula is returned unchanged. ``LtlfiltStats`` tracks cumulative
call count and wall time for diagnostics.

Called at the entry point of each external-tool wrapper (``run_ltl2tgba_for_counting``,
``RealizabilityChecker::check_realizability``,
``SatisfiabilityChecker::check_satisfiability``, and
``run_ganak_on_formula``) so that semantically equivalent formulae share
a single cache entry.

.. doxygenfile:: ltlfilt.hpp
