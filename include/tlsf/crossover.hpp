#pragma once

/// @file crossover.hpp
/// @brief Subformula-grafting crossover operator for tlsf::Specification.

#include "genetic/random_source.hpp"
#include "tlsf/specification.hpp"

/// Grafting crossover of @p parent_a and @p parent_b, following AuRUS. The
/// parents must share identical input/output signals; if they do not, @p
/// parent_a is returned unchanged. Otherwise the result starts as a copy of
/// @p parent_a and one conjunct per side is merged: for the assumption side
/// (INITIALLY, REQUIRE, ASSUME) and again for the guarantee side (PRESET,
/// ASSERT, GUARANTEE), a live conjunct of the result and a live conjunct of
/// @p parent_b are drawn uniformly and independently from anywhere on that
/// side, and with equal probability either a temporal subformula of the first
/// is replaced by one of the second, or the two are joined under one of
/// ∧, ∨, U, W. Everything else is @p parent_a's, so the offspring keeps its
/// section shape. Deleted conjuncts take no part on either side.
tlsf::Specification tlsf_crossover(const tlsf::Specification& parent_a,
                                   const tlsf::Specification& parent_b,
                                   const RandomSource& random_source);
