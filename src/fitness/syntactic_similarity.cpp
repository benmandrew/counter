#include "fitness/syntactic_similarity.hpp"

#include <cassert>
#include <vector>

#include "prop_formula.hpp"
#include "requirement.hpp"

namespace {

Formula conjoin_field(const Specification& spec, Formula Requirement::*field) {
    std::optional<Formula> conj;
    auto accumulate = [&](const std::vector<Requirement>& reqs) {
        for (const Requirement& req : reqs) {
            if (!conj) {
                conj = req.*field;
            } else {
                conj =
                    Formula::make_binary(Formula::Kind::And, *conj, req.*field);
            }
        }
    };
    accumulate(spec.m_assumptions);
    accumulate(spec.m_guarantees);
    assert(conj.has_value());
    return *conj;
}

Formula conjoin_triggers(const Specification& spec) {
    return conjoin_field(spec, &Requirement::m_trigger);
}

Formula conjoin_responses(const Specification& spec) {
    return conjoin_field(spec, &Requirement::m_response);
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
    assert((!specification.m_assumptions.empty() ||
            !specification.m_guarantees.empty()) &&
           (!other_specification.m_assumptions.empty() ||
            !other_specification.m_guarantees.empty()));
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
