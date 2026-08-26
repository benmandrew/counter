#pragma once

/// @file mutation.hpp
/// @brief Mutation operator for tlsf::Specification: rewrites one section
///        formula, either preserving its temporal skeleton or (following
///        Brizzio et al.) restructuring its temporal operators.

#include <cstdint>
#include <string>
#include <vector>

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "tlsf/specification.hpp"

/// Which way along the implication order a monotone rewrite moves the formula
/// it is given.
enum class MonotoneDirection : std::uint8_t {
    /// The result is implied by its parent.
    Weaken,
    /// The result implies its parent.
    Strengthen,
};

/// Which of the monotone rewrite's optional rules its menu offers. Both are
/// off by default, and off the rule list at every node is identical in content
/// and order to the one the binary held before these keys existed, so
/// `next_index` over it draws the same value and every draw after it follows.
struct MonotoneRules {
    /// `Config::tlsf_monotone_atom_rules`. On, adding a drawn literal as a
    /// disjunct (weakening) or conjunct (strengthening) is offered at every
    /// node, so an atom can grow; off it is offered at a conjunction or a
    /// disjunction alone, leaving an atom with nothing but the rewrite to a
    /// constant.
    bool atom_rules;
    /// `Config::tlsf_monotone_extra_rules`. On, `Release`, `Next` and the
    /// strengthening of a biconditional each gain a rule; off those three
    /// nodes offer the rewrite to a constant alone, so `R` sits among the
    /// kinds the temporal rewrite draws with no monotone move of its own
    /// while its duals `U` and `W` each carry one.
    bool extra_rules;
};

/// Rewrites @p formula so that the result is comparable to it under
/// implication: `Weaken` returns a formula @p formula implies, `Strengthen`
/// one that implies @p formula. One node is drawn uniformly from those whose
/// polarity is determinate — an occurrence under a biconditional is monotone
/// in neither direction and is not a site — and one rule applicable at that
/// node is then drawn uniformly. The rules are AuRUS's, from its
/// `FormulaWeakening` and `FormulaStrengthening` visitors: a subformula to
/// `true`/`false`, dropping a conjunct/disjunct, adding a drawn literal as a
/// disjunct/conjunct, `G φ` to `F φ` or `G F φ`, `F φ` to `G φ`, `U` to `W`
/// and back, and `φ ↔ ψ` to one of its two implications.
///
/// Polarity is what makes the guarantee hold through nesting: a weaker
/// subformula weakens the whole only where it occurs positively, so under a
/// negation or in the antecedent of an implication the dual rule is applied
/// instead. @p atoms is the section-appropriate atom pool and must be
/// non-empty.
///
/// @p rules says which of the two optional menu widenings are on; see
/// MonotoneRules. The parameter carries no default argument on purpose, and
/// neither field a default member initialiser: a new call site must state
/// which arm it wants rather than silently taking one.
Formula tlsf_monotone_rewrite(const Formula& formula,
                              MonotoneDirection direction, MonotoneRules rules,
                              const std::vector<std::string>& atoms,
                              const RandomSource& random_source);

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
/// rewrite (tlsf_monotone_rewrite) instead of either of those two, its
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
