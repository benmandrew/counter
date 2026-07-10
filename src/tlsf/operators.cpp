#include "tlsf/operators.hpp"

#include <cstddef>
#include <vector>

#include "prop_formula.hpp"
#include "tlsf/crossover.hpp"
#include "tlsf/mutation.hpp"

namespace {

using Section = std::vector<Formula>;

std::vector<Section*> mutable_sections_of(tlsf::Specification& spec) {
    return {&spec.m_initially, &spec.m_preset, &spec.m_require,
            &spec.m_assume,    &spec.m_assert, &spec.m_guarantee};
}

}  // namespace

tlsf::Specification tlsf_simplify(tlsf::Specification spec) {
    for (Section* section : mutable_sections_of(spec)) {
        for (Formula& formula : *section) {
            formula.simplify();
        }
    }
    return spec;
}

const GeneticOperators<tlsf::Specification>& tlsf_operators() {
    static const GeneticOperators<tlsf::Specification> ops{
        tlsf_crossover, tlsf_mutate, tlsf_simplify};
    return ops;
}
