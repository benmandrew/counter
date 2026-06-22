status_line.hpp
===============

An in-place terminal status line built from named columns. Each column is
registered once with ``add()`` and updated between renders with ``set()``.
Column widths grow monotonically to fit the widest value seen, so the layout
stays stable as values change. Padding on each render clears any characters
left over from a previously longer line.

Intended for per-generation progress output in the genetic repair loop,
where columns such as ``gen``, ``%``, ``time``, and per-filter drop counts
are registered at startup and updated after each generation.

.. doxygenfile:: status_line.hpp
