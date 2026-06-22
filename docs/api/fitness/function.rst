fitness/function.hpp
====================

Defines the fitness function types and the aggregation mechanism:

``FitnessFunction``
  A ``std::function<double(const Specification&)>`` returning a score in [0, 1].

``WeightedFitnessFunction``
  A ``FitnessFunction`` paired with a numeric weight and a display name.

``AggregateWeightedFitnessFunction``
  Combines multiple ``WeightedFitnessFunction`` instances into a single
  weighted-average scorer. Results are memoised by specification identity so
  each candidate is scored at most once per instance. Thread-safe: concurrent
  cache lookups take a shared lock; inserts take an exclusive lock.

``get_fitness_function(original_spec)`` builds the standard set from
``Config`` weights, omitting any component with a zero weight.

.. doxygenfile:: function.hpp
