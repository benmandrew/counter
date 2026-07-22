filter/well_separation.hpp
==========================

Detects specifications that are not *well-separated*: ones the system can
satisfy vacuously by forcing its own assumptions to fail. Realizability is
decided on ``(assumptions) -> (guarantees)``, so a candidate is satisfied for
free on any trace where the assumptions fail. A candidate is well-separated when
no system strategy can make the assumptions fail against every environment ---
equivalently, when the specification with its guarantees replaced by ``false``,
``(assumptions) -> false`` (that is, ``!(assumptions)``), is *unrealizable*. If
it is realizable, the system has a strategy that drives the output atoms so the
assumptions break, repairing nothing.

This is strictly stronger than the vacuity filter's satisfiability check:
assumptions can be perfectly satisfiable yet still forcibly falsifiable by the
system, because satisfiability treats every atom symmetrically whereas
realizability respects the input/output partition. Assumptions over input atoms
alone are always well-separated. Each test is a full ``ltlsynt`` query, so the
filter is off by default and interval-throttled when enabled. The check
short-circuits for specifications with no assumptions and treats a solver
timeout as unrealizable (well-separated), so a slow check never silently
discards a candidate.

.. doxygenfile:: well_separation.hpp
