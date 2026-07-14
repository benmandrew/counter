Configuration
=============

Algorithm parameters — population size, fitness weights, mutation rates and so
on — are tuned without recompiling by passing a *Tom's Obvious Minimal Language*
(TOML) file to ``--config``:

.. code-block:: sh

   counter --input spec.json --output-dir out --config my-config.toml

Every section and key is optional; absent keys keep their built-in defaults.

.. code-block:: toml

   [genetic]
   generations     = 20   # double the default evolution rounds
   population_size = 500

   [runtime]
   parallel = 16              # override thread pool size
   report_cpu_timing = true   # print a CPU-attribution report at the end

``example-config.toml`` in the repository root is an annotated template listing
every key with a comment explaining it. Its values are illustrative rather than
the defaults — consult the table below, or ``include/config.hpp``, for what a
key falls back to when omitted.

Fitness weights
---------------

Four components are combined into each candidate's score. Semantic similarity
and realisability status dominate; syntactic similarity breaks ties between
semantically comparable candidates, and the Halstead penalty holds back bloat.

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Key
     - Default
     - Component
   * - ``fitness.weight_semantic``
     - 0.5
     - Bounded model counting of satisfying LTL traces
   * - ``fitness.weight_status``
     - 0.5
     - ``ltlsynt`` outcome, mapped to {0, 0.1, 0.2, 0.5, 1.0}
   * - ``fitness.weight_syntactic``
     - 0.2
     - Shared sub-formula count, normalised to [0, 1]
   * - ``fitness.weight_halstead``
     - 0.1
     - Penalty for candidates larger than the original

Semantic similarity is the expensive one: it counts the satisfying traces of a
candidate up to ``model_counting.default_bound`` (default 20) using Ganak over
the transition matrices of SPOT-generated automata. Raising the bound sharpens
the measure and costs time.

Selection scheme
----------------

``genetic.selection_scheme`` decides how those four components drive selection.

**weighted** (default) collapses them into a single weighted average and ranks
by that scalar, using truncation selection with elitism. This finds the one
repair that best fits the configured trade-off, and is the right choice when the
weights already encode what a good repair means.

**nsga2** treats them as separate objectives and ranks candidates by
*Non-dominated Sorting Genetic Algorithm II*
(`NSGA-II <https://doi.org/10.1109/4235.996017>`_): Pareto non-domination first,
then crowding distance to spread the population along the front. It searches for
the whole Pareto front — the repairs not beaten on every objective at once —
rather than one weighted compromise. That is useful when the right balance
between, say, semantic similarity and size is not known in advance.

Two consequences are worth knowing. Under ``nsga2`` the ``[fitness]`` weights
only decide which components are active (weight > 0); they no longer bias
selection. And its survivor selection pools each generation's parents with their
offspring and keeps the best — a (μ+λ) scheme, already elitist — so
``elitism_rate = 0`` is the natural companion setting.

Both schemes still emit the weighted-average scalar in each repair's fitness
record, so outputs stay comparable across runs.

Filters
-------

``[filters]`` toggles the per-generation and final filters, and
``[filters.intervals]`` sets how often each runs, in generations. All four
default to 1 — every generation — and the final generation always runs every
filter regardless of interval, so the returned population is never left
un-deduplicated or un-weakened. Raising ``weakening``, the costliest of the
four, is the usual first move when a run is too slow.

Runtime
-------

``runtime.parallel`` overrides the thread pool size, which otherwise follows
``std::thread::hardware_concurrency()``. ``runtime.black_timeout_ms`` bounds
each ``black`` satisfiability query, defaulting to 1000 ms.

``runtime.report_cpu_timing`` prints a CPU-attribution report at the end,
separating time spent in counter's own code from time spent in the external
tools (``black``, ``ltlsynt``, ``ganak``), measured per-process via
``getrusage``/``wait4``. It defaults to false.
