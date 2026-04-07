#include "fitness/syntactic_similarity.hpp"

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
