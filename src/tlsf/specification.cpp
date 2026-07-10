#include "tlsf/specification.hpp"

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
    // TODO(strict): MealyStrict/MooreStrict currently reuse the standard
    // implication lowering; strict implication is a later refinement.
    bool assumption_has_term = false;
    const Formula assumption =
        collect_side(m_initially, m_require, m_assume, assumption_has_term);
    bool guarantee_has_term = false;
    const Formula guarantee =
        collect_side(m_preset, m_assert, m_guarantee, guarantee_has_term);
    if (!assumption_has_term) {
        return guarantee.to_string();
    }
    return Formula::make_binary(Formula::Kind::Implies, assumption, guarantee)
        .to_string();
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
