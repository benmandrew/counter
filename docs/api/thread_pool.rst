thread_pool.hpp
================

A fixed-size pool of worker threads consuming tasks from a shared queue.
Workers are spawned once at construction and reused for the pool's lifetime,
avoiding the per-task OS thread creation cost of ``std::async``. The pool
exposes a ``submit(callable)`` template that returns a ``std::future``.

``global_thread_pool()`` returns the process-lifetime pool shared by all
fitness-evaluation dispatch sites, sized to ``Config::n_hw_threads``. Callers
that need test isolation can construct their own ``ThreadPool`` instance.

.. doxygenfile:: thread_pool.hpp
