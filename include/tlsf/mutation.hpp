#pragma once

/// @file mutation.hpp
/// @brief Mutation operator for tlsf::Specification: rewrites one section
///        formula's propositional subtrees while preserving its temporal
///        skeleton.

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "tlsf/specification.hpp"

/// Probability of choosing an assumption-side section (INITIALLY, REQUIRE,
/// ASSUME) to mutate; the complementary probability selects a guarantee-side
/// section (PRESET, ASSERT, GUARANTEE).
inline constexpr double k_p_assumption = 0.3;
inline constexpr double k_p_guarantee = 0.7;

/// Mutates @p spec by rewriting exactly one section formula. A side is chosen
/// with probability k_p_assumption / k_p_guarantee (falling back to the other
/// side when the chosen one holds no formulae), then one formula is drawn
/// uniformly across that side's non-empty sections. Only the formula's
/// propositional subtrees are rewritten (via mutate_formula), so its temporal
/// operator skeleton is preserved. Assumption-side mutations draw atoms from
/// the inputs; guarantee-side mutations draw from inputs and outputs. If no
/// mutable formula exists the specification is returned unchanged.
tlsf::Specification tlsf_mutate(const tlsf::Specification& spec,
                                const RandomSource& random_source,
                                const Config& cfg);
