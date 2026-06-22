genetic/generation.hpp
=======================

The main driver of one generation of the genetic repair loop. Key types:

``FilterFunction``
  A named, stateful wrapper around a population-to-population callable that
  also tracks the input and output sizes of the most recent invocation for
  diagnostic output. Constructed implicitly from any compatible callable.

``ScoredSpecification``
  A ``Specification`` paired with its aggregated fitness score.

``evolve_generation`` runs truncation selection (sort by fitness, take top
*N* as parents), applies crossover and mutation to produce *N* offspring,
runs the per-generation filter pipeline, pads back to *N* if filters removed
too many, then scores the result. Progress callbacks are forwarded to the
scoring step.

Factory functions ``get_filter_functions`` and ``get_final_filter_functions``
assemble the standard filter sequences for in-generation and post-evolution use.

.. doxygenfile:: generation.hpp
