#include "fitness/semantic_similarity.hpp"

#include <cassert>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "fitness/model_counter.hpp"
#include "fitness/transfer_matrix.hpp"

namespace {

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

// Caches trace counts by (ltl, n_total_atoms, step_count): the matrix
// construction and exponentiation in count_traces is redone from scratch on
// every call, even though one side of a comparison is frequently the same
// requirement across an entire population/generation (e.g. the original
// specification's piece, compared against many mutated offspring).
Count cached_count_traces(const std::string& ltl, std::size_t n_total_atoms,
                          std::size_t step_count) {
    static std::unordered_map<std::string, Count> cache;
    static std::mutex cache_mutex;
    const std::string key = ltl + "|" + std::to_string(n_total_atoms) + "|" +
                            std::to_string(step_count);
    {
        std::scoped_lock lock(cache_mutex);
        const auto found = cache.find(key);
        if (found != cache.end()) {
            return found->second;
        }
    }
    const TransferSystem system =
        build_transfer_system_from_ltl(ltl, n_total_atoms);
    const Count count = count_traces(system, step_count);
    std::scoped_lock lock(cache_mutex);
    cache.emplace(key, count);
    return count;
}

SemanticSimilarityCounts count_semantic_similarity_terms(
    const Requirement& requirement, const Requirement& other_requirement,
    std::size_t step_count) {
    // All three systems must use the same atom universe so that the
    // conjunction count is always <= each individual count, keeping
    // the similarity ratio in [0, 1].
    const std::size_t n_atoms =
        count_joint_atoms(requirement, other_requirement);
    const std::string ltl = requirement_to_ltl(requirement);
    const std::string other_ltl = requirement_to_ltl(other_requirement);
    const std::string conjunction_ltl = "(" + ltl + ") & (" + other_ltl + ")";
    return {
        cached_count_traces(ltl, n_atoms, step_count),
        cached_count_traces(other_ltl, n_atoms, step_count),
        cached_count_traces(conjunction_ltl, n_atoms, step_count),
    };
}

double semantic_similarity_from_counts(const SemanticSimilarityCounts& counts) {
    if (counts.m_requirement_count == 0 &&
        counts.m_other_requirement_count == 0) {
        return 1.0;
    }
    if (counts.m_requirement_count == 0 ||
        counts.m_other_requirement_count == 0) {
        return 0.0;
    }
    const double first =
        ratio_or_throw(counts.m_conjunction_count, counts.m_requirement_count);
    const double second = ratio_or_throw(counts.m_conjunction_count,
                                         counts.m_other_requirement_count);
    // Harmonic mean of the two directional containment ratios: unlike the
    // arithmetic mean, this can't be pulled up to 0.5 by one ratio alone
    // hitting 1 (e.g. a requirement weakened to a tautology, which trivially
    // contains the original and makes second == 1 regardless of how small
    // first is). Mutually exclusive requirements give first == second == 0,
    // which the formula below would otherwise divide as 0/0.
    if (first == 0.0 && second == 0.0) {
        return 0.0;
    }
    return (2.0 * first * second) / (first + second);
}

}  // namespace

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count) {
    const SemanticSimilarityCounts counts = count_semantic_similarity_terms(
        requirement, other_requirement, step_count);
    return semantic_similarity_from_counts(counts);
}

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           const Config& cfg) {
    return semantic_similarity(requirement, other_requirement,
                               cfg.default_model_counting_bound);
}

double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           std::size_t step_count) {
    assert(specification.m_assumptions.size() ==
           other_specification.m_assumptions.size());
    assert(specification.m_guarantees.size() ==
           other_specification.m_guarantees.size());
    [[maybe_unused]] const std::size_t total_count =
        specification.m_assumptions.size() + specification.m_guarantees.size();
    assert(total_count > 0 && (other_specification.m_assumptions.size() +
                               other_specification.m_guarantees.size()) > 0);
    double total = 0.0;
    std::size_t changed_count = 0;
    // Requirement pairs that are identical contribute a trivial 1.0 and
    // dilute the average toward 1 as the specification grows, drowning out
    // the pairs that actually differ. Excluding them keeps the score
    // sensitive to the requirements a mutation actually touched.
    auto accumulate = [&](const std::vector<Requirement>& reqs1,
                          const std::vector<Requirement>& reqs2) {
        auto it1 = reqs1.begin();
        auto it2 = reqs2.begin();
        for (; it1 != reqs1.end(); ++it1, ++it2) {
            if (*it1 == *it2) {
                continue;
            }
            total += semantic_similarity(*it1, *it2, step_count);
            ++changed_count;
        }
    };
    accumulate(specification.m_assumptions, other_specification.m_assumptions);
    accumulate(specification.m_guarantees, other_specification.m_guarantees);
    if (changed_count == 0) {
        return 1.0;
    }
    return total / static_cast<double>(changed_count);
}

double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           const Config& cfg) {
    return semantic_similarity(specification, other_specification,
                               cfg.default_model_counting_bound);
}
