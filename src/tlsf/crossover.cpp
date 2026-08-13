#include "tlsf/crossover.hpp"

#include <cstddef>
#include <vector>

#include "prop_formula.hpp"

namespace {

using tlsf::sections_of;

// Section-wise uniform crossover needs matching section sizes; without them
// tlsf_crossover degrades to asexual reproduction (parent A returned
// unchanged). Since tlsf_add_assumption appends to m_assume, the first
// individual to gain an assumption can no longer cross with any other.
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
    const auto result_sections = tlsf::mutable_sections_of(result);
    const auto sections_b = sections_of(parent_b);
    for (std::size_t sec = 0; sec < result_sections.size(); ++sec) {
        for (std::size_t i = 0; i < result_sections[sec]->size(); ++i) {
            tlsf::SectionEntry& into = (*result_sections[sec])[i];
            const tlsf::SectionEntry& from = (*sections_b[sec])[i];
            // Deletion is mutation's move alone. Where either parent has
            // deleted this conjunct, parent A's slot stands unchanged, so
            // crossover can neither resurrect a deleted conjunct nor delete a
            // live one, and never breeds from content a parent threw away.
            // Same-shape parents can still differ here: they may have deleted
            // different conjuncts.
            if (into.m_removed || from.m_removed) {
                continue;
            }
            if (random_source.next_bool()) {
                into = from;
            }
        }
    }
    return result;
}
