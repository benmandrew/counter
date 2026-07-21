Counter
=======

Repairing unrealisable FRETish specifications.

The pages below are the curated public API — the ``include/`` headers. For a
deeper look at the implementation, the
`internal reference <internal/index.html>`_ is a separate full Doxygen build
covering ``src/`` as well as private class members, file-local (``static``)
functions, and anonymous namespaces, with source browsing and call graphs.

.. toctree::
   :maxdepth: 1
   :caption: Overview

   architecture
   configuration
   tlsf

.. toctree::
   :maxdepth: 1
   :caption: Core

   api/bounded_async
   api/config
   api/config_io
   api/prop_formula
   api/requirement
   api/serialisation
   api/status_line
   api/thread_pool

.. toctree::
   :maxdepth: 1
   :caption: Crash

   api/crash/crash_handler

.. toctree::
   :maxdepth: 1
   :caption: Fitness

   api/fitness/function
   api/fitness/halstead
   api/fitness/model_counter
   api/fitness/semantic_similarity
   api/fitness/status
   api/fitness/syntactic_similarity
   api/fitness/transfer_matrix

.. toctree::
   :maxdepth: 1
   :caption: Filter

   api/filter/bloat
   api/filter/implication
   api/filter/implication_check
   api/filter/vacuity

.. toctree::
   :maxdepth: 1
   :caption: Genetic

   api/genetic/crossover
   api/genetic/generation
   api/genetic/mutation
   api/genetic/nsga2
   api/genetic/operators
   api/genetic/random_source
   api/genetic/scored

.. toctree::
   :maxdepth: 1
   :caption: Runners

   api/runner/black
   api/runner/formaliser
   api/runner/ganak
   api/runner/ltlfilt
   api/runner/spot

.. toctree::
   :maxdepth: 1
   :caption: TLSF

   api/tlsf/specification
   api/tlsf/parser
   api/tlsf/writer
   api/tlsf/fitness
   api/tlsf/mucs
   api/tlsf/mutation
   api/tlsf/crossover
   api/tlsf/operators
   api/tlsf/filter
   api/tlsf/pipeline
