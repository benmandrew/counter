runner/black.hpp
=================

Thin wrapper around the ``black`` LTL satisfiability checker. The
``SatisfiabilityChecker`` class memoises results by formula string and applies
``Config::black_timeout`` to each subprocess invocation, returning
``std::nullopt`` on timeout. Cache hits take a shared lock; inserts take an
exclusive lock, making the checker thread-safe for concurrent filter passes.

``global_sat_checker()`` returns the process-lifetime instance shared by all
callers, so the memoisation cache accumulates across the entire run.

.. doxygenfile:: black.hpp
