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

Matching the tools
------------------

``spot_simplify`` asks for simplification level 3, which is what ``--simplify``
selects. That is not a detail that can be left to the library default: the
default options leave the containment checks off and so disagree with the tool
on about 5% of formulae.

``spot_translate_for_counting`` translates against a fresh ``bdd_dict`` per
call. Sharing one across calls is measurably no faster and renumbers atomic
propositions, because a dictionary carries over the propositions earlier
formulae registered.

Neither is byte-identical to the tool, and the reason is worth knowing before
relying on either. Spot prints the operands of commutative operators in
formula-node id order, and ids are assigned when a node is first interned — into
a table global to the process, which here lives for the whole run rather than
for one call. A cold process reproduces the tool exactly. After a handful of
unrelated calls, about a fifth of a random corpus prints differently, and the
translated automaton lists its atomic propositions in a different order with
every edge guard renumbered to match.

Both are renamings. The results are always logically equivalent and always
differ by ordering alone, which
``test/runner/differential_tests.cpp`` pins over a generated corpus, and
``scripts/check_engine_parity.py`` checks end-to-end across engines, thread
counts and repeated runs. The ``name:`` line differs too, and always did: the
tool fills it with its own rendering of the formula, which nothing here reads.

What does not move, and why
---------------------------

``ltlsynt`` does not move, and cannot. A per-call timeout in this project is
enforced by killing the process doing the work, in process there is nothing to
kill, and C++ offers no way to cancel a running call — and ``ltlsynt`` needs its
memory cap as much as its deadline.

``ltl2tgba`` faced the same objection and answers it differently.
``spot_translate_for_counting`` takes a deadline and honours it by *abandoning*
the call rather than stopping it: the work goes to a worker thread that carries
the ``libspot`` lock with it, and past the deadline the caller reports a timeout
while that worker runs on. Because it keeps the lock, every later call finds it
busy within the budget and spawns the tool instead, so one pathological formula
costs the process its fast path until that formula ends and nothing more. What
is given up is a thread and its memory for the duration.
``spot_abandoned_workers()`` reports how many are outstanding; it is worth
consulting before process exit, since static destruction underneath a live
worker tears down state it is still using.

Simplification never had a deadline to honour, so it moved unconditionally.

Spot 2.15.1 refuses to print the universal automaton it builds for a tautology,
reporting that the automaton is complete while its ``prop_complete()`` flag is
unset. That defect is in the library, not the command-line tool, so it surfaces
on both paths: as exit 2 from the binary, and as a thrown exception in process.
``SpotTranslation::m_tautology_print_bug`` reports it so the caller can
substitute the universal automaton, rather than let a genuinely-true formula
count against the run's scoring-failure tolerance.

.. doxygenfile:: spot_inprocess.hpp
