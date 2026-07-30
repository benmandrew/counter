dashboard.hpp
=============

Streams a run's progress to ``<output-dir>/progress.jsonl`` as it happens, and
copies the dashboard page beside it, so serving the output directory is all a
viewer needs::

   python3 -m http.server -d <output-dir> 8000

``DashboardWriter`` appends one JSON object per line and flushes each as it is
written — the page polls the file mid-run, so a buffered line is a line the
viewer cannot see. Four record types are emitted, each tagged with ``type``:
``run_start`` (input, generation and population counts, seed, objective names),
``stage`` (one per pipeline stage per generation, with its name, ``n_in``,
``n_out`` and elapsed time), ``generation`` (timing, best and mean fitness,
per-objective means, realizable count) and ``run_end``.

The stage names are not enumerated anywhere in the schema. The page derives them
from the ``stage`` records of the most recent generation, so a new filter or
pipeline stage appears without either side being taught about it.

Optional fields are omitted rather than defaulted. ``n_realizable`` is absent
when the driver does not measure it per generation — the TLSF path checks
realizability only at the end of a run — so the page can distinguish "not
measured" from "none found". ``muc_iter`` appears only under MUC-guided repair,
which restarts its generation count per core.

A writer that cannot open its file reports once and then does nothing: losing
the progress log must never take a repair run with it.

.. doxygenfile:: dashboard.hpp
