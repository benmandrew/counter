tlsf/crossover.hpp
==================

Subformula-*grafting* crossover for ``tlsf::Specification``, following AuRUS. Given two parents with identical signals, one conjunct per side is merged: for the assumption side (``INITIALLY``, ``REQUIRE``, ``ASSUME``) and again for the guarantee side (``PRESET``, ``ASSERT``, ``GUARANTEE``), a live conjunct of the first parent and a live conjunct of the second are drawn uniformly and independently from anywhere on that side. With probability 1/2 a temporal subformula of the first gives way to one of the second, and otherwise the two are joined under one of ∧, ∨, U, W. Everything else is the first parent's, so the offspring keeps its section shape and mismatched section sizes no longer stop two parents breeding. Mismatched signals leave the first parent unchanged.

A conjunct with no temporal subformula offers the whole conjunct as its one graft site, since ``INITIALLY``, ``PRESET``, ``REQUIRE`` and ``ASSERT`` are routinely propositional — their temporal operator comes from the lowering rather than the stored formula. AuRUS instead abandons the merge and drops the conjunct, which is not available here: a slot is positional, and deletion is mutation's move alone.

.. doxygenfile:: tlsf/crossover.hpp
