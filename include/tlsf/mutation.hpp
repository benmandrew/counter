#pragma once

/// @file mutation.hpp
/// @brief Mutation operator for tlsf::Specification: rewrites one section
///        formula's propositional subtrees while preserving its temporal
///        skeleton.

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "tlsf/specification.hpp"

/// Mutates @p spec by rewriting exactly one section formula. A side is chosen
/// with probability `cfg.tlsf_p_assumption` (assumption side: INITIALLY,
/// REQUIRE, ASSUME) versus `cfg.tlsf_p_guarantee` (guarantee side: PRESET,
/// ASSERT, GUARANTEE), falling back to the other side when the chosen one holds
/// no formulae, then one formula is drawn
/// uniformly across that side's non-empty sections. Only the formula's
/// propositional subtrees are rewritten (via mutate_formula), so its temporal
/// operator skeleton is preserved. Assumption-side mutations draw atoms from
/// the inputs; guarantee-side mutations draw from inputs and outputs. If no
/// mutable formula exists the specification is returned unchanged.
tlsf::Specification tlsf_mutate(const tlsf::Specification& spec,
                                const RandomSource& random_source,
                                const Config& cfg);
