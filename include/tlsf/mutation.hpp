#pragma once

/// @file mutation.hpp
/// @brief Mutation operator for tlsf::Specification: rewrites one section
///        formula, either preserving its temporal skeleton or (following
///        Brizzio et al.) restructuring its temporal operators.

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "tlsf/specification.hpp"

/// Mutates @p spec by rewriting exactly one section formula. A side is chosen
/// with probability `cfg.tlsf_p_assumption` (assumption side: INITIALLY,
/// REQUIRE, ASSUME) versus `cfg.tlsf_p_guarantee` (guarantee side: PRESET,
/// ASSERT, GUARANTEE), falling back to the other side when the chosen one holds
/// no formulae, then one formula is drawn uniformly across that side's
/// non-empty sections. With probability `cfg.tlsf_p_temporal` the chosen
/// formula is rewritten by the temporal-structure mutation (a recursive
/// re-implementation of Brizzio et al.'s operator, which may insert, drop, or
/// swap X/F/G/U/R/W nodes); otherwise only its propositional subtrees are
/// rewritten and the temporal operator skeleton is preserved. Assumption-side
/// mutations draw atoms from the inputs (or inputs ∪ outputs when
/// `cfg.allow_output_assumptions` is set); guarantee-side mutations draw from
/// inputs and outputs. If no mutable formula exists the specification is
/// returned unchanged.
///
/// With probability `cfg.p_add_assumption` the operator instead appends a new
/// environment assumption to the ASSUME section (an unconditional `G F <input>`
/// fairness property by default; a conditional `G(c -> F r)` over inputs ∪
/// outputs when `cfg.allow_output_assumptions` is set — see tlsf_add_assumption
/// in the .cpp).
tlsf::Specification tlsf_mutate(const tlsf::Specification& spec,
                                const RandomSource& random_source,
                                const Config& cfg);
