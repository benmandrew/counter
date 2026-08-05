tlsf/filter.hpp
===============

Population filters for ``tlsf::Specification``: a deduplication filter keeping
one representative per equal specification, and a vacuity guard dropping
specifications whose assumption-side conjunction is unsatisfiable — the TLSF
counterparts of the FRETISH filters, gated by the same configuration keys.

.. doxygenfile:: tlsf/filter.hpp
