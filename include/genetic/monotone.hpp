#pragma once

/// @file monotone.hpp
/// @brief Rewrites one node of a Formula so that the result is comparable to
///        the original under implication. Shared by both front ends: the TLSF
///        mutation operator offers it as a third rewrite arm, and the FRETISH
///        one offers it on a requirement's condition and response.

#include <cstdint>
#include <string>
#include <vector>

#include "genetic/random_source.hpp"
#include "prop_formula.hpp"

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
/// instead. @p atoms is the atom pool the caller's slot draws from and must be
/// non-empty.
///
/// The temporal rules are dead on a FRETISH condition or response, both of
/// which are propositional by construction; the propositional ones carry that
/// path.
///
/// @p rules says which of the two optional menu widenings are on; see
/// MonotoneRules. The parameter carries no default argument on purpose, and
/// neither field a default member initialiser: a new call site must state
/// which arm it wants rather than silently taking one.
Formula monotone_rewrite(const Formula& formula, MonotoneDirection direction,
                         MonotoneRules rules,
                         const std::vector<std::string>& atoms,
                         const RandomSource& random_source);
