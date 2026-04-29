#pragma once

#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"

/// Computes the status score of a requirement:
///
///   1.0  if A' ∧ G' is satisfiable and A' → G' is realisable
///   0.5  if A' ∧ G' is satisfiable but A' → G' is unrealisable
///   0.2  if A' ∧ G' is unsatisfiable, but A' and G' are individually
///        satisfiable
///   0.1  if A' is satisfiable but G' is not
///   0.0  if A' is unsatisfiable
///
/// where A' is the trigger and G' is the response of the requirement.
/// Satisfiability is checked using black; realisability is checked using
/// ltlsynt.
///
/// @param requirement    A requirement with m_ltl set
/// @param sat_checker    Satisfiability checker (cache is mutated)
/// @param real_checker   Realisability checker (cache is mutated)
/// @return               A status score in {0.0, 0.1, 0.2, 0.5, 1.0}
/// @throws std::invalid_argument if m_ltl is not set
double requirement_status(const Requirement& requirement,
                          SatisfiabilityChecker& sat_checker,
                          RealizabilityChecker& real_checker);
