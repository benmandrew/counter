#include "tlsf/crossover.hpp"

#include <cstddef>
#include <vector>

#include "prop_formula.hpp"

namespace {

using Section = std::vector<Formula>;

std::vector<const Section*> sections_of(const tlsf::Specification& spec) {
    return {&spec.m_initially, &spec.m_preset, &spec.m_require,
            &spec.m_assume,    &spec.m_assert, &spec.m_guarantee};
}

std::vector<Section*> mutable_sections_of(tlsf::Specification& spec) {
    return {&spec.m_initially, &spec.m_preset, &spec.m_require,
            &spec.m_assume,    &spec.m_assert, &spec.m_guarantee};
}

bool same_shape(const tlsf::Specification& parent_a,
                const tlsf::Specification& parent_b) {
    if (parent_a.m_inputs != parent_b.m_inputs ||
        parent_a.m_outputs != parent_b.m_outputs) {
        return false;
    }
    const auto sections_a = sections_of(parent_a);
    const auto sections_b = sections_of(parent_b);
    for (std::size_t i = 0; i < sections_a.size(); ++i) {
        if (sections_a[i]->size() != sections_b[i]->size()) {
            return false;
        }
    }
    return true;
}

}  // namespace

tlsf::Specification tlsf_crossover(const tlsf::Specification& parent_a,
                                   const tlsf::Specification& parent_b,
                                   const RandomSource& random_source) {
    if (!same_shape(parent_a, parent_b)) {
        return parent_a;
    }
    tlsf::Specification result = parent_a;
    const auto result_sections = mutable_sections_of(result);
    const auto sections_b = sections_of(parent_b);
    for (std::size_t sec = 0; sec < result_sections.size(); ++sec) {
        for (std::size_t i = 0; i < result_sections[sec]->size(); ++i) {
            if (random_source.next_bool()) {
                (*result_sections[sec])[i] = (*sections_b[sec])[i];
            }
        }
    }
    return result;
}
