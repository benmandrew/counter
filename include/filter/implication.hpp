#pragma once

#include "genetic/generation.hpp"
#include "runner/black.hpp"

/// Returns a FilterFunction that keeps only the maximal specifications of the
/// population under the implication partial order.
///
/// Spec A strictly dominates spec B when A logically implies B (A & !B is
/// unsatisfiable) but B does not imply A. Mutually equivalent specifications
/// (A implies B and B implies A) are both retained.
///
/// All n*(n-1) pairwise implication checks run in parallel. @p checker must
/// be thread-safe (SatisfiabilityChecker satisfies this). The checker is
/// captured by reference; it must outlive the returned FilterFunction.
///
/// @param checker  Satisfiability checker for pairwise implication tests
FilterFunction make_implication_filter(SatisfiabilityChecker& checker);
