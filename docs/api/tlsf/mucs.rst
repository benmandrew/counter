tlsf/mucs.hpp
=============

``tlsf::extract_muc`` extracts a **minimal unrealizable core (MUC)** from an
unrealizable :doc:`specification`: the smallest subset of the guarantee-side
sections (PRESET, ASSERT, GUARANTEE) that remains unrealizable when checked
against the full, unchanged environment side (INITIALLY, REQUIRE, ASSUME).
Relaxing an assumption can only make synthesis harder, so the environment side
is held fixed in every query and is never part of the core.

The extractor is QuickXplain (Junker 2004): a recursive divide-and-conquer that
needs O(k·log(n/k)) realizability queries for a core of size ``k`` out of ``n``
guarantee-side formulae. It relies on unrealizability being monotone in the
guarantee-side set — adding a system obligation can never restore realizability
— which makes a minimal unrealizable subset well defined.

The realizability oracle is injected as a ``std::function`` so the algorithm is
unit-testable without invoking ``ltlsynt``; a convenience overload binds the
process-wide :doc:`../runner/spot` checker. The ``mucs`` command-line tool wraps
this over a ``.tlsf`` file.

.. doxygenfile:: mucs.hpp
