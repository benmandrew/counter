genetic/crossover.hpp
======================

Crossover operators that combine two parent requirements or specifications to
produce an offspring. ``crossover_requirements`` independently selects the
condition, response, and timing from either parent with equal probability.
``crossover_specifications`` applies this pairwise to corresponding requirement
pairs and verifies that both parents share the same atom universes (in/out atoms
and requirement count), throwing ``std::invalid_argument`` otherwise.

.. doxygenfile:: crossover.hpp
