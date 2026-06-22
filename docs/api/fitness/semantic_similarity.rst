fitness/semantic_similarity.hpp
================================

Computes semantic similarity between requirements and specifications using
bounded model counting. The core metric is:

.. math::

   \text{sim}(r, r') = \frac{1}{2} \left(
     \frac{\text{shared}(r, r', k)}{\text{count}(r, k)} +
     \frac{\text{shared}(r, r', k)}{\text{count}(r', k)}
   \right)

where *shared(r, r', k)* is the number of traces of length *k* satisfying both
requirements and *count(r, k)* counts traces satisfying *r* alone. A score of
1.0 means identical trace semantics. Specification-level similarity averages
pairwise requirement similarities. The default bound *k* is
``Config::default_model_counting_bound``.

.. doxygenfile:: semantic_similarity.hpp
