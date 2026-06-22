genetic/random_source.hpp
==========================

``RandomSource`` wraps a ``std::function<size_t(size_t)>`` generator behind a
value-semantic handle with helpers ``next_index``, ``next_bool``, and
``next_real``. Storing the generator in a ``std::function`` allows tests to
inject deterministic sequences without template coupling throughout the genetic
operators. ``make_random_source_from_seed`` constructs a seeded ``std::mt19937``
source suitable for reproducible runs; the seed is stored and accessible via
``RandomSource::seed()``.

.. doxygenfile:: random_source.hpp
