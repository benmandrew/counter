#include "semantic_similarity.hpp"

#include <stdexcept>

#include "model_counter.hpp"
#include "transfer_matrix.hpp"

namespace {

constexpr std::size_t kDefaultModelCountingBound = 5;

double ratio_or_throw(Count numerator, Count denominator) {
    if (denominator == 0) {
        throw std::domain_error(
            "Semantic similarity is undefined when a requirement model count "
            "is zero.");
    }
    return static_cast<double>(static_cast<long double>(numerator) /
                               static_cast<long double>(denominator));
}

}  // namespace

SemanticSimilarityCounts count_semantic_similarity_terms(
    const Requirement& requirement, const Requirement& other_requirement,
    std::size_t step_count) {
    const TransferSystem system = build_transfer_system(requirement);
    const TransferSystem other_system =
        build_transfer_system(other_requirement);
    const CountMatrix conjunction_weighted =
        build_combined_weighted_transition_matrix(requirement,
                                                  other_requirement);
    TransferSystem conjunction_system;
    conjunction_system.m_states.assign(
        static_cast<std::size_t>(conjunction_weighted.rows()), State{});
    conjunction_system.m_transition_matrix = conjunction_weighted;
    conjunction_system.m_transition_matrix_is_weighted = true;
    return {
        count_traces(system, step_count),
        count_traces(other_system, step_count),
        count_traces(conjunction_system, step_count),
    };
}

double semantic_similarity_from_counts(const SemanticSimilarityCounts& counts) {
    return ratio_or_throw(counts.m_conjunction_count,
                          counts.m_requirement_count) +
           ratio_or_throw(counts.m_conjunction_count,
                          counts.m_other_requirement_count);
}

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count) {
    const SemanticSimilarityCounts counts = count_semantic_similarity_terms(
        requirement, other_requirement, step_count);
    return semantic_similarity_from_counts(counts);
}

double semantic_similarity(const Requirement& requirement1,
                           const Requirement& requirement2) {
    return semantic_similarity(requirement1, requirement2,
                               kDefaultModelCountingBound);
}
