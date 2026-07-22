#pragma once

/// @file well_separation.hpp
/// @brief Detection of specifications that are not well-separated: ones the
///        system can vacuously satisfy by forcing its own assumptions to fail.

#include "genetic/generation.hpp"
#include "requirement.hpp"
#include "runner/spot.hpp"

/// Returns whether the system can vacuously satisfy @p specification by
/// falsifying its own assumptions.
///
/// Realizability is decided on `(assumptions) -> (guarantees)`, so a candidate
/// is satisfied for free on any trace where the assumptions fail. A candidate
/// is *well-separated* when the system cannot force that outcome: no system
/// strategy makes the assumptions fail against every environment. Equivalently,
/// the specification obtained by replacing the guarantees with `false` --
/// `(assumptions) -> false`, i.e. `!(assumptions)` -- must be *unrealizable*.
/// If it is realizable, the system has a strategy that drives the output atoms
/// so the assumptions break, satisfying the original specification without
/// repairing anything.
///
/// This is strictly stronger than the vacuity filter's satisfiability check:
/// assumptions can be perfectly satisfiable yet still forcibly falsifiable by
/// the system, because satisfiability treats every atom symmetrically whereas
/// realizability respects the input/output partition. A specification whose
/// assumptions constrain only input atoms is always well-separated -- the
/// system controls no atom it could use to break them.
///
/// Conservative under uncertainty: a specification with no assumptions has
/// nothing to falsify and is reported well-separated without a solver call, and
/// an ltlsynt query that times out is treated as unrealizable (well-separated),
/// so a slow check never silently discards a candidate.
///
/// @param specification The specification to test
/// @param checker       Realizability checker for the ltlsynt query;
/// thread-safe
///                      for concurrent calls
bool specification_is_not_well_separated(const Specification& specification,
                                         RealizabilityChecker& checker);

/// Returns a filter dropping specifications that are not well-separated: ones
/// the system can vacuously satisfy by forcing its own assumptions to fail.
/// @p checker is captured by reference and must outlive the returned filter.
FilterFunction make_well_separation_filter(RealizabilityChecker& checker);
