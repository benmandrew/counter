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

/// Rewrites @p formula so that the result is comparable to it under
/// implication: `Weaken` returns a formula @p formula implies, `Strengthen`
/// one that implies @p formula. One node is drawn uniformly from those whose
/// polarity is determinate — an occurrence under a biconditional is monotone
/// in neither direction and is not a site — and one rule applicable at that
/// node is then drawn uniformly. The rules follow AuRUS's
/// `FormulaWeakening` and `FormulaStrengthening` visitors: a subformula to
/// `true`/`false`, adding a drawn literal as a disjunct/conjunct at any node,
/// dropping a conjunct/disjunct, `G φ` to `F φ` or `G F φ`, `F φ` to `G φ`,
/// `U` to `W` and back, `φ R ψ` to `ψ` or `G ψ`, `X φ` to `F φ` or `G φ`,
/// and `φ ↔ ψ` to one of its two implications or to `φ ∧ ψ` or `¬φ ∧ ¬ψ`.
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
Formula monotone_rewrite(const Formula& formula, MonotoneDirection direction,
                         const std::vector<std::string>& atoms,
                         const RandomSource& random_source);
