crash/crash_handler.hpp
=======================

Installs signal handlers for ``SIGSEGV``, ``SIGABRT``, and ``SIGFPE`` that
capture a raw stack trace in a signal-safe manner, then fork-exec a
``signal_tracer`` helper to resolve symbols and write a full crash report to
``crashes/<pid>_<timestamp>.log``. The crash log includes the ``--seed`` and
full ``Config`` values needed to reproduce the run. Call ``init_cpptrace`` once
from ``main`` with ``argv[0]`` so the handler can locate the ``signal_tracer``
binary in the same directory. Use ``register_crash_metadata`` to attach the
seed and config string before starting the evolution loop.

.. doxygenfile:: crash_handler.hpp
