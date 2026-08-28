#include "guarantee_parts.hpp"

#include <cassert>
#include <cstddef>
#include <utility>
#include <vector>

#include "prop_formula.hpp"
#include "tlsf/mucs.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

namespace {

// Section ids for the guarantee (system) side, in the order the MUC extractor
// enumerates them. Kept in step with guarantee_side_candidates in mucs.cpp.
constexpr std::size_t k_preset = 1;
constexpr std::size_t k_assert = 4;
constexpr std::size_t k_guarantee = 5;

// split_into is total: whatever it is handed comes back as one or more parts
// covering it. So a node whose kind says it has operands but which does not
// carry them is an indivisible part rather than a case to assert on, and the
// checks below say that rather than documenting a precondition an NDEBUG build
// would drop. The kind tests do make the operands present; what the checks buy
// is that a violation is a slightly worse partition instead of a dereference
// of an empty optional.
void split_into(const Formula& formula, std::vector<Formula>& out) {
    if (formula.kind() == Formula::Kind::And) {
        const auto children = formula.binary_children();
        if (children) {
            split_into(children->first, out);
            split_into(children->second, out);
            return;
        }
    }
    if (formula.kind() == Formula::Kind::Globally) {
        const auto child = formula.unary_child();
        if (child && child->kind() == Formula::Kind::And) {
            // G distributes over conjunction, so this rewrite preserves the
            // language. It is the one that matters in practice: a TLSF
            // GUARANTEE is commonly a single G over a wide conjunction.
            const auto inner = child->binary_children();
            if (inner) {
                split_into(
                    Formula::make_unary(Formula::Kind::Globally, inner->first),
                    out);
                split_into(
                    Formula::make_unary(Formula::Kind::Globally, inner->second),
                    out);
                return;
            }
        }
    }
    out.push_back(formula);
}

// Deleted conjuncts contribute no parts: the MRS walk grades what the
// specification still asks the system to do, and they ask nothing.
void append_section(const Section& section, std::size_t section_id,
                    std::vector<CoreFormula>& out) {
    for (const Formula& formula : live_formulae(section)) {
        std::vector<Formula> parts;
        split_into(formula, parts);
        for (Formula& part : parts) {
            out.push_back({section_id, std::move(part)});
        }
    }
}

}  // namespace

std::vector<CoreFormula> split_guarantee_parts(const Specification& spec) {
    std::vector<CoreFormula> parts;
    parts.reserve(spec.m_preset.size() + spec.m_assert.size() +
                  spec.m_guarantee.size());
    append_section(spec.m_preset, k_preset, parts);
    append_section(spec.m_assert, k_assert, parts);
    append_section(spec.m_guarantee, k_guarantee, parts);
    return parts;
}

Specification build_part_subset(const Specification& spec,
                                const std::vector<CoreFormula>& parts,
                                const std::vector<std::size_t>& indices) {
    Specification subset;
    subset.m_title = spec.m_title;
    subset.m_description = spec.m_description;
    subset.m_inputs = spec.m_inputs;
    subset.m_outputs = spec.m_outputs;
    // The environment side is carried over whole. Relaxing an assumption can
    // only make synthesis harder, so it never belongs in a subset walk asking
    // what the system can still achieve -- the same reason the MUC extractor
    // holds it fixed.
    subset.m_initially = spec.m_initially;
    subset.m_require = spec.m_require;
    subset.m_assume = spec.m_assume;
    for (const std::size_t index : indices) {
        assert(index < parts.size());
        const CoreFormula& part = parts[index];
        switch (part.section_id) {
            case k_preset:
                subset.m_preset.emplace_back(part.formula);
                break;
            case k_assert:
                subset.m_assert.emplace_back(part.formula);
                break;
            default:
                subset.m_guarantee.emplace_back(part.formula);
                break;
        }
    }
    return subset;
}

}  // namespace tlsf
