bounded_async.hpp
==================

Provides ``run_bounded_async``, a template helper that runs up to *N* futures
concurrently across a sequence of tasks. Unlike a naive ``std::async`` loop it
collects whichever future finishes first rather than always waiting on the
oldest, so a single slow task cannot stall others that are already complete.
Used by the fitness-scoring layer to dispatch Ganak model-counting queries with
bounded parallelism.

.. doxygenfile:: bounded_async.hpp
