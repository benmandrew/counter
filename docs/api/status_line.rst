status_line.hpp
===============

An in-place terminal status line built from named columns. Each column is registered once with ``add()`` and updated between renders with ``set()``. Column widths grow monotonically to fit the widest value seen, so the layout stays stable as values change. Padding on each render clears any characters left over from a previously longer line.

Only on a terminal. ``render()`` returns immediately when ``stdout_is_tty()`` is false, and ``finish()`` alone emits the line, trimmed of its padding — so a redirected run gets exactly one committed line per generation and no escape codes at all. That free function is the guard everything moving the cursor must consult, because a file keeps every frame a terminal would have overwritten: one unguarded status line was 59,455 bytes of escape codes carrying 1,225 bytes of content, and ``scripts/run_experiments.py`` redirects every run it starts.

A column may be declared *transient*, via the second parameter of ``add()``, meaning it belongs to the live display but not to the committed line. A within-unit percentage is the motivating case: it is the whole point of the line while the unit runs, and reads 100% on every committed line by construction.

Intended for per-generation progress output in the genetic repair loop, where ``gen``, a transient ``%``, ``time``, the best fitness and the realizable count are registered at startup and updated after each generation. A committed line reads ``gen: 1/10  time: 0.31s  best: 0.971  real: 8``.

.. doxygenfile:: status_line.hpp
