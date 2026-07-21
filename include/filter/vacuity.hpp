#pragma once

/// @file vacuity.hpp
/// @brief Detection of vacuously-realizable specifications, whose assumptions
///        are jointly unsatisfiable.

#include "genetic/generation.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"

/// Returns whether @p specification's assumptions are jointly unsatisfiable.
///
/// Realizability is decided on `(assumptions) -> (guarantees)`, so a
/// specification whose assumptions contradict one another is realizable for
/// free: a false antecedent makes the implication a tautology regardless of
/// the guarantees. Such a specification is not a repair, and the weakening
/// filter cannot reject it — an unsatisfiable assumption implies every other
/// assumption, so it passes every implication test.
///
/// Only assumptions are checked. Unsatisfiable *guarantees* need no guard:
/// they make the implication unsatisfiable and so are already reported
/// unrealizable, which the search punishes on its own.
///
/// Conservative under uncertainty: a specification with no assumptions, or one
/// whose satisfiability check times out, is reported as not vacuous, so a slow
/// check never silently discards a candidate.
///
/// @param specification The specification to test
/// @param checker       Satisfiability checker for the LTL query; must be
///                      thread-safe when called concurrently
bool specification_has_unsatisfiable_assumptions(
    const Specification& specification, SatisfiabilityChecker& checker);

/// Returns a filter dropping specifications that are vacuously realizable
/// because their assumptions are jointly unsatisfiable. @p checker is captured
/// by reference and must outlive the returned filter.
FilterFunction make_vacuity_filter(SatisfiabilityChecker& checker);
