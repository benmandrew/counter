spot_simplify.hpp
=================

``spot_simplify`` simplifies an LTL formula in process, through the linked
``libspot``, in place of spawning ``ltlfilt --simplify``. It is what
``simplify_ltl`` calls under the default ``SimplifyEngine::Libspot``.

Simplification is the only Spot tool this project can move in process. The
others cannot: ``ltl2tgba`` and ``ltlsynt`` are given per-call deadlines, and
those work by killing a separate process. In process there is nothing to kill,
and C++ offers no way to cancel a running call. Simplification never had a
deadline, so moving it gives up nothing that existed.

The output is byte-identical to the command-line tool because the simplifier is
asked for level 3, which is what ``--simplify`` selects. This is not a detail
that can be left to the library default: the default options disagree with
``--simplify`` on about 5% of formulae, because level 3 additionally enables
the containment checks.

Every call is serialised behind one process-wide mutex, and the single shared
simplifier is constructed under that same mutex. That requirement was measured
rather than assumed. The contended state is not inside ``tl_simplifier`` at
all — it is process-global underneath it, in Spot's Bison and Flex parser
globals and in the ``robin_hood`` table that interns formula nodes. A
per-thread simplifier therefore still reaches all of it and crashes, and it
crashes even when every call is locked, because construction alone is unsafe.

Serialising costs nothing at this scale. A call is roughly 0.02 ms against the
8 ms of process startup it replaces, so even fully serialised the work is a
rounding error next to the execs it removes.

.. doxygenfile:: spot_simplify.hpp
