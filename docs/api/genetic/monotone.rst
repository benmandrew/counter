genetic/monotone.hpp
====================

Rewrites one node of a ``Formula`` so that the result is comparable to the original under implication: ``Weaken`` returns a formula the original implies, ``Strengthen`` one that implies the original. The site is drawn uniformly over the nodes whose polarity is determinate, and the rule uniformly over those applicable at that node. ``MonotoneRules`` says which of the two optional menu widenings the caller wants. Both front ends call it — the TLSF mutation operator as a third rewrite arm beside the temporal and propositional ones, the FRETISH one on a requirement's condition and response.

.. doxygenfile:: genetic/monotone.hpp
