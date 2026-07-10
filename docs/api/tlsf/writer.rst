tlsf/writer.hpp
===============

``tlsf::write`` pretty-prints a ``tlsf::Specification`` back to valid
basic-format TLSF text: an ``INFO`` block (TITLE, DESCRIPTION, SEMANTICS,
TARGET) and a ``MAIN`` block (INPUTS, OUTPUTS, then each non-empty section with
its formulae as ``;``-terminated lines rendered via ``Formula::to_string``). The
output round-trips through :doc:`parser`.

.. doxygenfile:: writer.hpp
