tlsf/filter.hpp
===============

Population filters for ``tlsf::Specification``: a deduplication filter keeping one representative per equal specification, and a vacuity guard dropping specifications that carry a trivial section literal (``false`` in INITIALLY/REQUIRE/ASSUME, ``true`` in PRESET/ASSERT/GUARANTEE), or a *valid* guarantee-section formula, or an unsatisfiable assumption-side conjunction — the TLSF counterparts of the FRETISH filters, gated by the same configuration keys.

.. doxygenfile:: tlsf/filter.hpp
