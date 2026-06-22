config.hpp
==========

Central repository of compile-time tuning knobs for the genetic repair
algorithm. All parameters are ``static constexpr`` members of ``Config`` except
``black_timeout`` (mutable so tests can relax it on slow CI machines) and
``n_hw_threads`` (queried at startup). Changing a value here recompiles all
translation units that include this header; no separate configuration file is
needed.

.. doxygenfile:: config.hpp
