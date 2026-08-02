spot_inprocess.hpp
==================

The calls this project makes into the linked ``libspot``, in place of spawning
the equivalent Spot command-line tool. There are two: ``spot_simplify`` stands
in for ``ltlfilt --simplify``, and ``spot_translate_for_counting`` stands in for
``ltl2tgba -D -S -H``.

Both live in one header for one reason: every call into ``libspot`` has to be
serialised behind the *same* process-wide lock. Splitting them across headers
would make it easy to add a third entry point with a lock of its own, which is
no protection at all.

Why the lock is not optional
----------------------------

The contended state is not in any Spot object the caller holds. It is
process-global underneath: Spot's Bison and Flex parser globals, and the
``robin_hood`` table that interns formula nodes. Giving each thread its own
simplifier or translator therefore does not help. It crashes — and it crashes
even when every call is locked, because construction alone reaches that state.
This was measured rather than assumed; the four configurations tried, and what
each did, are recorded in ``PROFILING.md``.

Serialising is affordable because of the ratio involved. What is removed is
about 8 ms of process startup per call. What is serialised is 0.02 ms of
simplification, or 0.16 ms of translation, on real workloads.

Matching the tools exactly
--------------------------

``spot_simplify`` asks for simplification level 3, which is what ``--simplify``
selects. That is not a detail that can be left to the library default: the
default options leave the containment checks off and so disagree with the tool
on about 5% of formulae.

``spot_translate_for_counting`` translates against a fresh ``bdd_dict`` per
call. Sharing one across calls is measurably no faster and renumbers atomic
propositions, because a dictionary carries over the propositions earlier
formulae registered. Its output matches the tool apart from the ``name:`` line,
which the tool fills with its own simplified rendering of the formula and which
nothing here reads.

What does not move, and why
---------------------------

``ltl2tgba`` moves only when no deadline is asked for, and ``ltlsynt`` does not
move at all. A per-call timeout in this project is enforced by killing the
process doing the work, and in process there is nothing to kill — C++ offers no
way to cancel a running call. So ``run_ltl2tgba_for_counting`` keeps spawning
whenever ``ltl2tgba_timeout_ms`` is set, and falls to the in-process path only
on the default of zero. Simplification never had a deadline, so it moved
unconditionally.

Spot 2.15.1 refuses to print the universal automaton it builds for a tautology,
reporting that the automaton is complete while its ``prop_complete()`` flag is
unset. That defect is in the library, not the command-line tool, so it surfaces
on both paths: as exit 2 from the binary, and as a thrown exception in process.
``SpotTranslation::m_tautology_print_bug`` reports it so the caller can
substitute the universal automaton, rather than let a genuinely-true formula
count against the run's scoring-failure tolerance.

.. doxygenfile:: spot_inprocess.hpp
