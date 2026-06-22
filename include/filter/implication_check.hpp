#pragma once

/// @file implication_check.hpp
/// @brief Pairwise assume-guarantee implication check between two
///        specifications, used by the implication and weakening filters.

#include <optional>

#include "requirement.hpp"
#include "runner/black.hpp"

/// Returns whether spec `from` logically implies spec `dest`, using a
/// sufficient assume-guarantee decomposition: each of from's assumptions must
/// be implied by some assumption of dest (dest assumes no more than from
/// requires), and each of dest's guarantees must be implied by some guarantee
/// of from.
///
/// Returns true if implication is confirmed, false if it is definitively
/// refuted, or nullopt if some individual check timed out and the result is
/// uncertain (the implication may or may not hold).
///
/// Under-detects implication: implications that only hold via a combination of
/// several requirements are missed. Definite false returns are conservative
/// (a spec that is actually dominated may be retained), never false positives.
///
/// @param from     The candidate stronger specification.
/// @param dest     The candidate weaker specification.
/// @param checker  Satisfiability checker for LTL queries; must be
///                 thread-safe when called concurrently.
std::optional<bool> spec_implies(const Specification& from,
                                 const Specification& dest,
                                 SatisfiabilityChecker& checker);
