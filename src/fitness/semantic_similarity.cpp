#include "fitness/semantic_similarity.hpp"

#include <cassert>
#include <vector>

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

double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           std::size_t step_count) {
    assert(specification.m_assumptions.size() ==
           other_specification.m_assumptions.size());
    assert(specification.m_guarantees.size() ==
           other_specification.m_guarantees.size());
    const std::size_t total_count =
        specification.m_assumptions.size() + specification.m_guarantees.size();
    assert(total_count > 0 && (other_specification.m_assumptions.size() +
                               other_specification.m_guarantees.size()) > 0);
    double total = 0.0;
    auto accumulate = [&](const std::vector<Requirement>& reqs1,
                          const std::vector<Requirement>& reqs2) {
        auto it1 = reqs1.begin();
        auto it2 = reqs2.begin();
        for (; it1 != reqs1.end(); ++it1, ++it2) {
            total += semantic_similarity(*it1, *it2, step_count);
        }
    };
    accumulate(specification.m_assumptions, other_specification.m_assumptions);
    accumulate(specification.m_guarantees, other_specification.m_guarantees);
    return total / static_cast<double>(total_count);
}

double semantic_similarity(const Specification& specification,
                           const Specification& other_specification) {
    return semantic_similarity(specification, other_specification,
                               kDefaultModelCountingBound);
}
