#pragma once

/// @file mutation.hpp
/// @brief Mutation operator for tlsf::Specification: rewrites one section
///        formula, either preserving its temporal skeleton or (following
///        Brizzio et al.) restructuring its temporal operators.

#include <string>
#include <vector>

#include "config.hpp"
#include "genetic/monotone.hpp"
#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "tlsf/specification.hpp"

/// Mutates @p spec by rewriting exactly one section formula. The assumption
/// side (INITIALLY, REQUIRE, ASSUME) is chosen with probability
/// `cfg.tlsf_p_assumption` and the guarantee side (PRESET, ASSERT, GUARANTEE)
/// with the complement. The chosen side falls back to the other when it holds
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
/// `cfg.tlsf_connective_implies` widens the temporal rewrite's case (2d)
/// graft, which fires at an atom or a unary node, to draw an implication as
/// its connective beside `U`, `W`, `&` and `|`. Off -- the default -- a
/// guarded response such as `p -> X phi` takes more than one draw to reach at
/// the nodes where a guard has to be introduced.
///
/// With probability `cfg.tlsf_p_monotone` the chosen formula takes a monotone
/// rewrite (monotone_rewrite) instead of either of those two, its
/// direction drawn as a fair coin and its rule menu widened by
/// `cfg.tlsf_monotone_atom_rules` and `cfg.tlsf_monotone_extra_rules`.
///
/// With probability `cfg.p_add_assumption` the operator instead appends a new
/// environment assumption to the ASSUME section (a conditional `G(c -> F r)`
/// over inputs ∪ outputs when `cfg.allow_output_assumptions` is set, which is
/// the default; an unconditional `G F <input>` fairness property with it off —
/// see tlsf_add_assumption in the .cpp). With probability
/// `cfg.tlsf_p_clone_assumption` that appended assumption is instead a copy of
/// an existing live ASSUME conjunct, which later generations mutate; the
/// template stands in when there is nothing live to copy.
tlsf::Specification tlsf_mutate(const tlsf::Specification& spec,
                                const RandomSource& random_source,
                                const Config& cfg);
