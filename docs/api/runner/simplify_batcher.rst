runner/simplify_batcher.hpp
===========================

Coalesces concurrent LTL simplification misses into a single ``ltlfilt``
exec. Process startup dominates the cost of a simplification -- roughly 9ms
and 2700 minor page faults, independent of the formula -- so a batch of
formulae pays it once rather than once per formula.

There is no timer and no artificial delay. Whichever caller finds no leader
takes everything queued at that moment and runs it; the others wait, so the
next batch is as large as the contention warrants and collapses to a batch of
one when a single thread is asking. ``ltlfilt_batchers`` sets how many batches
may be in flight at once, since one leader would serialise every
simplification in the process behind one exec at a time.

Two safety properties are load-bearing. ``ltlfilt`` is run with
``--skip-errors``, which guarantees one reply line per request line, and a
batch whose reply count does not match its request count is rejected outright
-- a misattributed simplification would corrupt the search rather than slow it
down. And a batch is capped in bytes so its whole input fits in a pipe buffer,
because the leader writes the batch before draining any answers and would
otherwise deadlock against its own child.

The child is spawned through :doc:`process`, under
``ParentDeathPolicy::KillWithParentThread``: one leader thread forks it, feeds
it, drains it and reaps it inside a single call, which is the shape PDEATHSIG
is tied to.

.. doxygenfile:: simplify_batcher.hpp
