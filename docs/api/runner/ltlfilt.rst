runner/ltlfilt.hpp
==================

Normalises LTL formulae to a canonical form using SPOT's ``ltlfilt`` tool. ``normalize_ltl`` memoises results so the subprocess runs at most once per unique input string. On binary inaccessibility or subprocess failure the original formula is returned unchanged. ``LtlfiltStats`` tracks cumulative call count and wall time for diagnostics.

``simplify_ltl`` is the live entry point, called by ``SatisfiabilityChecker::check_satisfiability`` on a cache miss to build that cache's normalised key. The other tool wrappers take a ``formula_key`` key instead: the pass blows up super-exponentially on the deep nested-X conjunctions ``run_ltl2tgba_for_counting`` and ``RealizabilityChecker::check_realizability`` ask about, and it eliminates a variable wherever one term subsumes another, which ``run_ganak_on_formula`` cannot afford because its caller multiplies the count over the wider alphabet. ``normalize_ltl`` has had no caller outside its own tests since that last move.

.. doxygenfile:: ltlfilt.hpp
