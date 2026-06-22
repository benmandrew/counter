fitness/model_counter.hpp
=========================

``count_traces(system, step_count)`` counts the number of valid bounded traces
in a requirement automaton by computing **e** · **T**\ :sup:`k` · **1**, where
**e** is the initial-state indicator vector, **T** is the (valuation-weighted)
transition matrix from the ``TransferSystem``, and *k* = *step_count*. This is
the core primitive for semantic similarity: comparing how many traces of bounded
length satisfy each requirement and their conjunction.

.. doxygenfile:: model_counter.hpp
