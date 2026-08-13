#include "tlsf/operators.hpp"

#include <cstddef>
#include <vector>

#include "prop_formula.hpp"
#include "tlsf/crossover.hpp"
#include "tlsf/mutation.hpp"

tlsf::Specification tlsf_simplify(tlsf::Specification spec) {
    for (tlsf::Section* section : tlsf::mutable_sections_of(spec)) {
        for (tlsf::SectionEntry& entry : *section) {
            // Deleted conjuncts are left as they are. Nothing reads their
            // content again, so rewriting it buys nothing.
            if (entry.m_removed) {
                continue;
            }
            entry.m_formula.simplify();
        }
    }
    return spec;
}

const GeneticOperators<tlsf::Specification>& tlsf_operators() {
    static const GeneticOperators<tlsf::Specification> ops{
        tlsf_crossover, tlsf_mutate, tlsf_simplify};
    return ops;
}
