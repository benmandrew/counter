filter/implication_check.hpp
============================

Provides ``spec_implies(from, dest, checker)``, which tests whether
specification *from* logically implies *dest* using a sufficient
assume-guarantee decomposition: each assumption of *from* must be implied by
some assumption of *dest*, and each guarantee of *dest* must be implied by some
guarantee of *from*. This is intentionally incomplete (it under-detects
implication) but produces no false positives, so dominated specs may be
retained but correct specs are never dropped. Called by both the weakening and
implication filters.

.. doxygenfile:: implication_check.hpp
