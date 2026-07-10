#pragma once

/// @file crossover.hpp
/// @brief Positional crossover operator for tlsf::Specification.

#include "genetic/random_source.hpp"
#include "tlsf/specification.hpp"

/// Positional per-section crossover of @p parent_a and @p parent_b. Requires
/// the two parents to share identical input/output signals and identical
/// section sizes; if they do not, @p parent_a is returned unchanged. Otherwise
/// the result starts as a copy of @p parent_a (keeping its signals and
/// semantics) and, for each section index, adopts @p parent_b's formula with
/// probability 1/2.
tlsf::Specification tlsf_crossover(const tlsf::Specification& parent_a,
                                   const tlsf::Specification& parent_b,
                                   const RandomSource& random_source);
