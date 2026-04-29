#pragma once

#include "requirement.hpp"

/// Computes the status score of a specification:
///
///   1.0  if A' ∧ G' is satisfiable and A' → G' is realisable
///   0.5  if A' ∧ G' is satisfiable but A' → G' is unrealisable
///   0.2  if A' ∧ G' is unsatisfiable, but A' and G' are individually
///        satisfiable
///   0.1  if the conjunction of triggers is satisfiable but that of responses
///        is not
///   0.0  if the conjunction of triggers is unsatisfiable
///
/// where A' is the conjunction of triggers and G' is the conjunction of
/// responses across all requirements. Satisfiability is checked using black;
/// realisability is checked using ltlsynt.
///
/// @param specification A specification whose requirements all have m_ltl set
/// @return              A status score in {0.0, 0.1, 0.2, 0.5, 1.0}
/// @throws std::invalid_argument if any requirement lacks m_ltl
double specification_status(const Specification& specification);
