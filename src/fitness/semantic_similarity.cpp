#include "fitness/semantic_similarity.hpp"

#include <cassert>

#include "fitness/model_counter.hpp"
#include "fitness/transfer_matrix.hpp"

namespace {

constexpr std::size_t kDefaultModelCountingBound = 5;

double ratio_or_throw(Count numerator, Count denominator) {
    assert(denominator != 0);
    return static_cast<double>(static_cast<long double>(numerator) /
                               static_cast<long double>(denominator));
}

struct SemanticSimilarityCounts {
    Count m_requirement_count;
    Count m_other_requirement_count;
    Count m_conjunction_count;
};

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
    double first =
        ratio_or_throw(counts.m_conjunction_count, counts.m_requirement_count);
    double second = ratio_or_throw(counts.m_conjunction_count,
                                   counts.m_other_requirement_count);
    return (first + second) * 0.5;
}

}  // namespace

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
