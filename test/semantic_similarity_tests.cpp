#include <cmath>
#include <stdexcept>

#include "requirement.hpp"
#include "semantic_similarity.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_semantic_similarity_identical_requirements_score_two() {
    const Requirement requirement{"P", "Q", Timing::Immediately};
    const double score = semantic_similarity(requirement, requirement, 1);
    expect(std::fabs(score - 2.0) < 1e-12,
           "semantic-similarity: identical requirements should have score 2");
}

void test_semantic_similarity_formula_value_explicit_step_count() {
    const Requirement requirement{"P", "Q", Timing::Immediately};
    const Requirement other_requirement{"P", "P|Q", Timing::Immediately};
    const double score = semantic_similarity(requirement, other_requirement, 1);
    expect(std::fabs(score - 1.75) < 1e-12,
           "semantic-similarity: expected score 1.75 from formula");
}

void test_semantic_similarity_default_overload_matches_explicit_step_count() {
    const Requirement requirement{"P", "Q", Timing::Immediately};
    const Requirement other_requirement{"P", "P|Q", Timing::Immediately};
    const double with_default =
        semantic_similarity(requirement, other_requirement);
    const double with_explicit_step_count =
        semantic_similarity(requirement, other_requirement, 5);
    expect(std::fabs(with_default - with_explicit_step_count) < 1e-12,
           "semantic-similarity: default overload should use step_count=5");
}

void test_semantic_similarity_rejects_zero_denominator() {
    const Requirement unsatisfiable_requirement{"A|!A", "A&!A",
                                                Timing::Immediately};
    const Requirement other_requirement{"A", "A", Timing::Immediately};
    bool threw = false;
    try {
        (void)semantic_similarity(unsatisfiable_requirement, other_requirement,
                                  1);
    } catch (const std::domain_error&) {
        threw = true;
    }
    expect(threw,
           "semantic-similarity: zero denominator should throw domain_error");
}

}  // namespace

void run_semantic_similarity_tests() {
    test_semantic_similarity_identical_requirements_score_two();
    test_semantic_similarity_formula_value_explicit_step_count();
    test_semantic_similarity_default_overload_matches_explicit_step_count();
    test_semantic_similarity_rejects_zero_denominator();
}
