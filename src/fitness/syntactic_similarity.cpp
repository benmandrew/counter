#include "fitness/syntactic_similarity.hpp"

#include <stdexcept>

#include "prop_formula.hpp"
#include "requirement.hpp"

namespace {

Formula conjoin_triggers(const Specification& spec) {
    auto it = spec.m_requirements.begin();
    Formula conj = it->m_trigger;
    for (++it; it != spec.m_requirements.end(); ++it) {
        conj = Formula::make_binary(Formula::Kind::And, conj, it->m_trigger);
    }
    return conj;
}

Formula conjoin_responses(const Specification& spec) {
    auto it = spec.m_requirements.begin();
    Formula conj = it->m_response;
    for (++it; it != spec.m_requirements.end(); ++it) {
        conj = Formula::make_binary(Formula::Kind::And, conj, it->m_response);
    }
    return conj;
}

}  // namespace

double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement) {
    double condition_similarity =
        requirement.m_trigger.syntactic_similarity(other_requirement.m_trigger);
    double response_similarity = requirement.m_response.syntactic_similarity(
        other_requirement.m_response);
    double timing_similarity =
        1.0;  // Placeholder until timing similarity is implemented
    return (condition_similarity + response_similarity + timing_similarity) /
           3.0;
}

double syntactic_similarity(const Specification& specification,
                            const Specification& other_specification) {
    if (specification.m_requirements.empty() ||
        other_specification.m_requirements.empty()) {
        throw std::invalid_argument(
            "Specifications must be non-empty to compute syntactic "
            "similarity.");
    }
    const double trigger_similarity =
        conjoin_triggers(specification)
            .syntactic_similarity(conjoin_triggers(other_specification));
    const double response_similarity =
        conjoin_responses(specification)
            .syntactic_similarity(conjoin_responses(other_specification));
    double timing_similarity =
        1.0;  // Placeholder until timing similarity is implemented
    return (trigger_similarity + response_similarity + timing_similarity) / 3.0;
}
