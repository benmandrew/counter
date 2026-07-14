TLSF specifications
===================

Alongside FRETISH, counter repairs basic *Temporal Logic Synthesis Format*
(TLSF) specifications directly — the interchange format used by the
reactive-synthesis community. A ``.tlsf`` input is auto-detected from the file
extension, or forced with ``--format tlsf``.

In TLSF mode the same genetic machinery evolves the six specification sections —
``INITIALLY``, ``PRESET``, ``REQUIRE``, ``ASSUME``, ``ASSERT`` and
``GUARANTEE`` — rather than FRETISH requirements. The fitness function mirrors
the FRETISH one (semantic similarity, realisability status, syntactic
similarity, and a Halstead size penalty) but scores whole TLSF formulae. See
:doc:`configuration` for the weights.

Repairs are written back as valid TLSF, so a repair can be fed straight into
``realize``, ``ltl``, or an external synthesiser. Each ``repair_N.tlsf`` is
paired with a ``repair_N.fitness.json`` holding its score.

The arbiter example
-------------------

The bundled ``examples/arbiter-gr1`` is a two-client mutual-exclusion arbiter.
Its guarantees demand that each client is granted infinitely often (``G F g0``,
``G F g1``), that a grant only follows a request, and that no two grants happen
at once:

.. code-block:: text

   INPUTS  { r0; r1; }
   OUTPUTS { g0; g1; }
   GUARANTEE {
     G (g0 -> r0);
     G (g1 -> r1);
     G !(g0 & g1);
     G F g0;
     G F g1;
   }

This is unrealisable. Nothing compels the environment to keep issuing requests,
so if a client falls silent forever the system can never grant it, and
``G F g0`` fails through no fault of the system:

.. code-block:: sh

   realize examples/arbiter-gr1/spec.tlsf   # UNREALIZABLE

Two ways out
------------

The classic fix strengthens the environment with *request fairness* — assume
each client requests infinitely often — which is what
``examples/arbiter-gr1/ideal.tlsf`` does:

.. code-block:: text

   ASSUME {
     G F r0;
     G F r1;
   }

But that is not the only way to make the specification realisable, and counter
does not privilege it. Weakening the guarantees works too: an arbiter that need
not grant anyone is trivially implementable. Both moves appear in a single run.

.. code-block:: sh

   counter --input examples/arbiter-gr1/spec.tlsf --output-dir out --seed 42

That run produces 19 maximal repairs, taking 9.97 s and 30.34 s on two
successive runs (release build, 20 threads). The seed fixes which repairs come
out, not how long they take — that swings with how the external solvers get
scheduled.

Fitness sorts the two strategies apart. The top repair (0.877) recovers both
fairness assumptions:

.. code-block:: text

   ASSUME {
     G(X(F(G(!(G(r0))))));
     G(F(!(r0)));
     G(F(r1));      <- request fairness for client 1
     G(X(r1));
     G(F(r0));      <- request fairness for client 0
   }

It finds the intended move, but does not find it cleanly: three redundant
assumptions ride along, and ``G(X(r1))`` is far stronger than needed, pinning
``r1`` true at every step after the first. Semantic similarity tolerates them
because they cost little in trace count relative to the realisability gain.

Lower-ranked repairs take the other route, mangling the guarantees instead —
this one (0.575) inverts ``G F g0`` into its own negation:

.. code-block:: text

   GUARANTEE {
     G(true);
     G((g1) -> (r1));
     G(!((g0) & (g1)));
     G(F(!(g0)));    <- was G F g0
     G(F(g0));
   }

Both are genuinely realisable — ``realize`` confirms every output — so the
ranking, not the filter, is what separates a useful repair from a degenerate
one. Reading down the fitness order is the intended way to use the output.

The assumption-adding move is controlled by ``p_add_assumption``, shared with
FRETISH mode; ``[tlsf.mutation]`` tunes the TLSF-specific operators. Both are
described in :doc:`configuration`.
