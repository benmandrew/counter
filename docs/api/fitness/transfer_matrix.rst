fitness/transfer_matrix.hpp
============================

The ``TransferSystem`` type represents a finite-state automaton for bounded
trace model counting. Construction (via ``build_transfer_system``) calls
``ltl2tgba`` to generate a deterministic HOA automaton from a requirement's LTL
formula, then calls Ganak to compute per-transition valuation counts, producing
a ``CountMatrix`` whose (i, j) entry is the number of satisfying valuations on
the transition from state *i* to state *j*.

``count_joint_atoms`` and ``build_conjunction_transfer_system`` extend the
mechanism to pairs of requirements, enabling the numerator
*shared(r, r', k)* in the semantic similarity formula to be computed.

``Count`` is either ``uint64_t`` or ``unsigned __int128`` (configured at build
time). All arithmetic on ``Count`` values must use ``count_add_overflow`` /
``count_mul_overflow`` with an assertion on the overflow flag.

.. doxygenfile:: transfer_matrix.hpp
