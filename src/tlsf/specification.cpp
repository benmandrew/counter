#include "tlsf/specification.hpp"

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "prop_formula.hpp"

namespace tlsf {

namespace {

/// Folds a section into a single Formula with left-associative And. An empty
/// section is the constant `true`; a singleton is returned unchanged.
Formula conj(const std::vector<Formula>& section) {
    if (section.empty()) {
        return Formula("true");
    }
    Formula result = section.front();
    for (std::size_t idx = 1; idx < section.size(); ++idx) {
        result = Formula::make_binary(Formula::Kind::And, result, section[idx]);
    }
    return result;
}

/// G(conj(section)); the caller guarantees the section is non-empty.
Formula globally_of(const std::vector<Formula>& section) {
    return Formula::make_unary(Formula::Kind::Globally, conj(section));
}

/// `(⋀ antecedent) → (⋀ consequent)` with the paper's `true` simplifications:
/// an empty consequent leaves the implication trivially true (⇒ nullopt, so the
/// caller omits it); an empty antecedent collapses it to the consequent alone.
std::optional<Formula> side_implication(
    const std::vector<Formula>& antecedent,
    const std::vector<Formula>& consequent) {
    if (consequent.empty()) {
        return std::nullopt;
    }
    if (antecedent.empty()) {
        return conj(consequent);
    }
    return Formula::make_binary(Formula::Kind::Implies, conj(antecedent),
                                conj(consequent));
}

/// Collects the non-empty side terms, G-wrapping the invariant section, and
/// conjoins them. Reports whether any term was contributed at all.
Formula collect_side(const std::vector<Formula>& verbatim_first,
                     const std::vector<Formula>& invariant,
                     const std::vector<Formula>& verbatim_last,
                     bool& any_term) {
    std::vector<Formula> terms;
    if (!verbatim_first.empty()) {
        terms.push_back(conj(verbatim_first));
    }
    if (!invariant.empty()) {
        terms.push_back(
            Formula::make_unary(Formula::Kind::Globally, conj(invariant)));
    }
    if (!verbatim_last.empty()) {
        terms.push_back(conj(verbatim_last));
    }
    any_term = !terms.empty();
    return conj(terms);
}

}  // namespace

std::string Specification::to_ltl() const {
    // TLSF v1.1 standard lowering (paper §3.2):
    //   θ_e → ( θ_s ∧ ( (G ψ_e ∧ φ_e) → (G ψ_s ∧ φ_s) ) )
    // Strict pulls the system invariant ψ_s into a weak-until guard and drops
    // it from the consequent (it must hold at least as long as ψ_e does):
    //   θ_e → ( θ_s ∧ (ψ_s W ¬ψ_e) ∧ ( (G ψ_e ∧ φ_e) → φ_s ) )
    // θ_e=INITIALLY, θ_s=PRESET, ψ_e=REQUIRE, ψ_s=ASSERT, φ_e=ASSUME,
    // φ_s=GUARANTEE; an absent section is `true` and drops out.
    const bool strict = m_semantics == Semantics::MealyStrict ||
                        m_semantics == Semantics::MooreStrict;

    std::vector<Formula> antecedent;  // G ψ_e ∧ φ_e
    if (!m_require.empty()) {
        antecedent.push_back(globally_of(m_require));
    }
    if (!m_assume.empty()) {
        antecedent.push_back(conj(m_assume));
    }

    std::vector<Formula> consequent;  // G ψ_s ∧ φ_s (ψ_s omitted when strict)
    if (!strict && !m_assert.empty()) {
        consequent.push_back(globally_of(m_assert));
    }
    if (!m_guarantee.empty()) {
        consequent.push_back(conj(m_guarantee));
    }

    std::vector<Formula>
        body;  // θ_s ∧ (ψ_s W ¬ψ_e) ∧ (antecedent → consequent)
    if (!m_preset.empty()) {
        body.push_back(conj(m_preset));
    }
    if (strict && !m_assert.empty()) {
        body.push_back(Formula::make_binary(
            Formula::Kind::WeakUntil, conj(m_assert),
            Formula::make_unary(Formula::Kind::Not, conj(m_require))));
    }
    if (std::optional<Formula> inner =
            side_implication(antecedent, consequent)) {
        body.push_back(*inner);
    }

    const Formula body_formula = conj(body);  // empty body ⇒ true
    if (m_initially.empty()) {
        return body_formula.to_string();
    }
    return Formula::make_binary(Formula::Kind::Implies, conj(m_initially),
                                body_formula)
        .to_string();
}

std::string Specification::assumption_ltl() const {
    bool any_term = false;
    return collect_side(m_initially, m_require, m_assume, any_term).to_string();
}

std::string Specification::guarantee_ltl() const {
    bool any_term = false;
    return collect_side(m_preset, m_assert, m_guarantee, any_term).to_string();
}

namespace {

auto tie_sections(const Specification& spec) {
    return std::tie(spec.m_semantics, spec.m_inputs, spec.m_outputs,
                    spec.m_initially, spec.m_preset, spec.m_require,
                    spec.m_assume, spec.m_assert, spec.m_guarantee);
}

}  // namespace

bool operator==(const Specification& lhs, const Specification& rhs) {
    return tie_sections(lhs) == tie_sections(rhs);
}

bool operator<(const Specification& lhs, const Specification& rhs) {
    return tie_sections(lhs) < tie_sections(rhs);
}

}  // namespace tlsf
