filter/bloat.hpp
================

Provides ``make_bloat_cap_filter``, a population filter that drops any
specification containing a formula (trigger or response) larger than a ratio
multiple of the largest formula in the original. Checking per-formula rather
than per-specification prevents a bloated formula in one requirement from
escaping detection when diluted by smaller formulas elsewhere. Used as an
optional per-generation filter to prevent formula bloat during mutation.

.. doxygenfile:: bloat.hpp
