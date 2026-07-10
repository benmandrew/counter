#include "tlsf/mutation.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "genetic/mutation.hpp"
#include "prop_formula.hpp"

namespace {

using Section = std::vector<Formula>;

// The three section vectors on one side of the specification, paired so a
// chosen formula can be located and rewritten in place.
std::vector<Section*> side_sections(tlsf::Specification& spec,
                                    bool assumption_side) {
    if (assumption_side) {
        return {&spec.m_initially, &spec.m_require, &spec.m_assume};
    }
    return {&spec.m_preset, &spec.m_assert, &spec.m_guarantee};
}

std::size_t side_formula_count(const std::vector<Section*>& sections) {
    std::size_t total = 0;
    for (const Section* section : sections) {
        total += section->size();
    }
    return total;
}

// Mutates only the maximal propositional subtrees of @p formula, treating every
// temporal node as a fixed boundary that is reconstructed verbatim around its
// (recursively mutated) children. This preserves the temporal skeleton exactly:
// mutate_formula is only ever applied to a subtree with no temporal operators,
// so it can neither introduce nor drop an X/F/G/U/R/W node. Applying
// mutate_formula to the whole formula would not be safe — its propositional
// rewrites can discard a child, which would delete a nested temporal subtree.
Formula mutate_propositional_parts(const Formula& formula,
                                   const std::vector<std::string>& atoms,
                                   const RandomSource& random_source) {
    if (formula.is_propositional()) {
        return mutate_formula(formula, atoms, random_source);
    }
    switch (formula.kind()) {
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally: {
            const auto child = formula.unary_child();
            if (!child.has_value()) {
                return formula;
            }
            return Formula::make_unary(
                formula.kind(),
                mutate_propositional_parts(*child, atoms, random_source));
        }
        default: {
            const auto children = formula.binary_children();
            if (!children.has_value()) {
                return formula;
            }
            return Formula::make_binary(
                formula.kind(),
                mutate_propositional_parts(children->first, atoms,
                                           random_source),
                mutate_propositional_parts(children->second, atoms,
                                           random_source));
        }
    }
}

}  // namespace

tlsf::Specification tlsf_mutate(const tlsf::Specification& spec,
                                const RandomSource& random_source,
                                const Config& cfg) {
    (void)cfg;
    tlsf::Specification mutated = spec;

    bool assumption_side = random_source.next_real() < k_p_assumption;
    std::vector<Section*> sections = side_sections(mutated, assumption_side);
    if (side_formula_count(sections) == 0) {
        // Fall back to the other side when the chosen one is empty.
        assumption_side = !assumption_side;
        sections = side_sections(mutated, assumption_side);
    }
    const std::size_t total = side_formula_count(sections);
    if (total == 0) {
        return spec;
    }

    // Guarantee-side atom pool is inputs ∪ outputs; assumption-side is inputs.
    std::vector<std::string> pool = mutated.m_inputs;
    if (!assumption_side) {
        pool.insert(pool.end(), mutated.m_outputs.begin(),
                    mutated.m_outputs.end());
    }
    if (pool.empty()) {
        // Without atoms, mutate_formula's structural rewrites cannot draw a
        // replacement atom; leave the specification unchanged.
        return spec;
    }

    std::size_t choice = random_source.next_index(total);
    for (Section* section : sections) {
        if (choice < section->size()) {
            (*section)[choice] = mutate_propositional_parts(
                (*section)[choice], pool, random_source);
            return mutated;
        }
        choice -= section->size();
    }
    return spec;
}
