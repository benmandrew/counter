filter/implication.hpp
======================

Population filters based on the logical implication partial order between
specifications:

``make_weakening_filter``
  Per-generation filter. Discards any candidate that is not logically implied
  by the original specification (i.e. it preserves only weakenings).

``make_dedup_filter``
  Drops structurally identical duplicate specifications, keeping the first
  occurrence in input order.

``make_implication_filter``
  Final-pass filter. Keeps only the maximal elements of the population under
  the implication order — specs that are not strictly dominated by any other
  survivor. All *n(n-1)/2* pairwise checks run in parallel via the thread-safe
  ``SatisfiabilityChecker``.

``ImplicationFilterStats`` exposes atomic counters for comparisons, skips,
duplicates, and timeouts from the most recent implication-filter sweep.

.. doxygenfile:: implication.hpp
