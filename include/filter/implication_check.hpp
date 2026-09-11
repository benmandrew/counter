#pragma once

/// @file implication_check.hpp
/// @brief Pairwise implication check between two whole specifications, used
///        by the implication and weakening filters.

#include <optional>

#include "requirement.hpp"
#include "runner/black.hpp"

/// Returns whether spec `from` logically implies spec `dest`, as one query
/// over the two whole-specification lowerings: `from.to_ltl() & !dest.to_ltl()`
/// is unsatisfiable exactly when the implication holds.
///
/// Returns true if implication is confirmed, false if it is definitively
/// refuted, or nullopt if the check timed out and the result is uncertain.
///
/// Exact, so a false return is a fact about the two specifications rather than
/// a limit of the check. It decomposed per requirement until 2026-09-11 --
/// each of dest's guarantees implied by *some single* guarantee of from, and
/// the assumptions the other way round -- which missed every implication
/// holding only via several requirements together, and asked 9 to 26 queries
/// per pair where this asks one. `tlsf_spec_implies` has had this shape since
/// the TLSF path existed, and the two now agree.
///
/// @param from     The candidate stronger specification.
/// @param dest     The candidate weaker specification.
/// @param checker  Satisfiability checker for LTL queries; must be
///                 thread-safe when called concurrently.
std::optional<bool> spec_implies(const Specification& from,
                                 const Specification& dest,
                                 SatisfiabilityChecker& checker);
