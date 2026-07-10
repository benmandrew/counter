tlsf/parser.hpp
===============

``tlsf::parse`` is a recursive-descent parser for basic-format TLSF. It accepts
the ``INFO { ... } MAIN { ... }`` structure, the INFO metadata keys, the
INPUTS/OUTPUTS signal lists, and the specification subsections, building each
statement into a temporal ``Formula`` via the Formula factories. Bounded and
timed operators (``X[n]``, ``F[a..b]``, ``G[a..b]``) are expanded into
next-operator chains at parse time, capped at ``k_max_bound_expansion``.

Full-format constructs (GLOBAL/PARAMETERS/DEFINITIONS blocks, bus and
enumeration declarations, loop aggregates, primed/bus-access syntax) are
rejected with a ``std::invalid_argument`` pointing at ``syfco -f basic``
pre-lowering.

.. doxygenfile:: parser.hpp
