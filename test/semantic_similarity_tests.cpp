#include <cmath>
#include <stdexcept>

#include "requirement.hpp"
#include "semantic_similarity.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_semantic_similarity_counts_identical_requirements() {
    const Requirement requirement{"P", "Q", Timing::Immediately};
    const SemanticSimilarityCounts counts =
        count_semantic_similarity_terms(requirement, requirement, 1);

    expect(counts.m_requirement_count == 3,
           "semantic-similarity: expected #(S,1)=3 for P->Q immediately");
    expect(counts.m_other_requirement_count == 3,
           "semantic-similarity: expected #(S',1)=3 for identical requirement");
    expect(counts.m_conjunction_count == 3,
           "semantic-similarity: expected #(S and S',1)=3 for identical "
           "requirements");
}

void test_semantic_similarity_formula_value() {
    const Requirement requirement{"P", "Q", Timing::Immediately};
    const Requirement other_requirement{"P", "P|Q", Timing::Immediately};
    const SemanticSimilarityCounts counts =
        count_semantic_similarity_terms(requirement, other_requirement, 1);

    expect(counts.m_requirement_count == 3,
           "semantic-similarity: expected #(S,1)=3 for P->Q immediately");
    expect(counts.m_other_requirement_count == 4,
           "semantic-similarity: expected #(S',1)=4 for P->(P|Q) immediately");
    expect(counts.m_conjunction_count == 3,
           "semantic-similarity: expected #(S and S',1)=3");

    const double score = semantic_similarity_from_counts(counts);
    expect(std::fabs(score - 1.75) < 1e-12,
           "semantic-similarity: expected score 1.75 from formula");
}

void test_semantic_similarity_rejects_zero_denominator() {
    bool threw = false;
    try {
        (void)semantic_similarity_from_counts({0, 1, 0});
    } catch (const std::domain_error&) {
        threw = true;
    }
    expect(threw,
           "semantic-similarity: zero denominator should throw domain_error");
}

}  // namespace

void run_semantic_similarity_tests() {
    test_semantic_similarity_counts_identical_requirements();
    test_semantic_similarity_formula_value();
    test_semantic_similarity_rejects_zero_denominator();
}
