#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "fitness/semantic_similarity.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_semantic_similarity_identical_requirements_score_two() {
    const Requirement requirement{Formula("P"), Formula("Q"),
                                  timing::immediately()};
    const double score = semantic_similarity(requirement, requirement, 1);
    expect(std::fabs(score - 1.0) < 1e-12,
           "semantic-similarity: identical requirements should have score 1");
}

void test_semantic_similarity_formula_value_explicit_step_count() {
    const Requirement requirement{Formula("P"), Formula("Q"),
                                  timing::immediately()};
    const Requirement other_requirement{Formula("P"), Formula("P|Q"),
                                        timing::immediately()};
    const double score = semantic_similarity(requirement, other_requirement, 1);
    // (P -> Q) traces are a strict subset of (P -> P|Q) traces, so the
    // conjunction count equals the (P -> Q) count exactly: first == 1.0,
    // second == 0.75, harmonic mean == 2*1.0*0.75/1.75 == 6/7.
    expect(std::fabs(score - (6.0 / 7.0)) < 1e-12,
           "semantic-similarity: expected score 6/7 from formula");
}

void test_semantic_similarity_default_overload_matches_explicit_step_count() {
    const Requirement requirement{Formula("P"), Formula("Q"),
                                  timing::immediately()};
    const Requirement other_requirement{Formula("P"), Formula("P|Q"),
                                        timing::immediately()};
    const double with_default =
        semantic_similarity(requirement, other_requirement);
    const double with_explicit_step_count = semantic_similarity(
        requirement, other_requirement, Config::default_model_counting_bound);
    expect(std::fabs(with_default - with_explicit_step_count) < 1e-12,
           "semantic-similarity: default overload should use "
           "Config::default_model_counting_bound");
}

void test_semantic_similarity_identical_specifications_score_one() {
    const Specification spec(
        {},
        {Requirement{Formula("P"), Formula("Q"), timing::immediately()},
         Requirement{Formula("P"), Formula("!Q"), timing::immediately()}},
        {"P"}, {"Q"});
    const double score = semantic_similarity(spec, spec, 1);
    expect(std::fabs(score - 1.0) < 1e-12,
           "semantic-similarity: identical specifications should have score 1");
}

void test_semantic_similarity_specification_averages_requirements() {
    const Requirement req_imm{Formula("P"), Formula("Q"),
                              timing::immediately()};
    const Requirement req_next{Formula("P"), Formula("Q"),
                               timing::next_timepoint()};
    const Requirement req_within{Formula("P"), Formula("Q"),
                                 timing::within_ticks(3)};
    const Requirement req_for{Formula("P"), Formula("Q"), timing::for_ticks(2)};
    const Requirement req_after{Formula("P"), Formula("Q"),
                                timing::after_ticks(2)};
    // req_within is identical in both specs and should be excluded from the
    // average; req_imm/req_next and req_for/req_after differ and should be
    // averaged together.
    const Specification spec1({}, {req_imm, req_within, req_for}, {"P"}, {"Q"});
    const Specification spec2({}, {req_next, req_within, req_after}, {"P"},
                              {"Q"});
    const double score = semantic_similarity(spec1, spec2, 1);
    const double expected = (semantic_similarity(req_imm, req_next, 1) +
                             semantic_similarity(req_for, req_after, 1)) /
                            2.0;
    expect(std::fabs(score - expected) < 1e-12,
           "semantic-similarity: specification score should average only the "
           "requirement pairs that differ, excluding unchanged pairs");
}

// A requirement weakened to a tautology (trigger entails response trivially,
// so it accepts every trace) trivially contains the original's accepting
// set, making the "recall" ratio exactly 1 regardless of how small the
// original's footprint is. Under the old arithmetic mean of the two
// directional ratios this floored the score at 0.5 no matter how vacuous the
// weakening was; the harmonic mean lets a near-zero "precision" ratio pull
// the score down toward 0 instead.
void test_semantic_similarity_tautology_scores_near_zero() {
    const Requirement original{Formula("P"), Formula("Q"),
                               timing::after_ticks(1)};
    const Requirement tautology{Formula("Q"), Formula("Q"),
                                timing::within_ticks(2)};
    const double score = semantic_similarity(tautology, original, 20);
    expect(score < 0.01,
           "semantic-similarity: a requirement weakened to a tautology "
           "should score near 0, not float at the old 0.5 floor");
}

// Two distinct eventually-timing requirements. Their conjunction produces a
// generalized-Buchi automaton; the old HOA parser over-counted accepting states
// and returned similarity > 1. Verifies the result stays in [0, 1].
void test_semantic_similarity_liveness_in_range() {
    const Requirement req_a{Formula("P"), Formula("Q"), timing::eventually()};
    // A, B avoid SPOT's reserved LTL operator letters (e.g. R for Release).
    const Requirement req_b{Formula("A"), Formula("B"), timing::eventually()};
    const double identical = semantic_similarity(req_a, req_a, 4);
    expect(std::fabs(identical - 1.0) < 1e-12,
           "semantic-similarity: identical eventually reqs must score 1");
    const double cross = semantic_similarity(req_a, req_b, 4);
    expect(cross >= 0.0 && cross <= 1.0,
           "semantic-similarity: eventually cross-score must lie in [0, 1]");
}

// Checks that all timing variants produce similarity in [0, 1] when compared
// cross-requirement. This exercises both safety and liveness automaton paths.
void test_semantic_similarity_all_timings_in_range() {
    const Requirement other{Formula("P"), Formula("A"), timing::immediately()};
    const std::vector<std::pair<std::string, Requirement>> cases = {
        {"immediately",
         Requirement(Formula("P"), Formula("Q"), timing::immediately())},
        {"next-timepoint",
         Requirement(Formula("P"), Formula("Q"), timing::next_timepoint())},
        {"within-ticks",
         Requirement(Formula("P"), Formula("Q"), timing::within_ticks(2))},
        {"for-ticks",
         Requirement(Formula("P"), Formula("Q"), timing::for_ticks(2))},
        {"after-ticks",
         Requirement(Formula("P"), Formula("Q"), timing::after_ticks(1))},
        {"eventually",
         Requirement(Formula("P"), Formula("Q"), timing::eventually())},
    };
    for (const auto& [label, req] : cases) {
        const double self_score = semantic_similarity(req, req, 2);
        expect(
            std::fabs(self_score - 1.0) < 1e-12,
            "semantic-similarity: " + label + " identical reqs must score 1");
        const double cross_score = semantic_similarity(req, other, 2);
        expect(
            cross_score >= 0.0 && cross_score <= 1.0,
            "semantic-similarity: " + label + " cross score must be in [0, 1]");
    }
}

}  // namespace

void run_semantic_similarity_tests() {
    test_semantic_similarity_identical_requirements_score_two();
    test_semantic_similarity_formula_value_explicit_step_count();
    test_semantic_similarity_default_overload_matches_explicit_step_count();
    test_semantic_similarity_identical_specifications_score_one();
    test_semantic_similarity_specification_averages_requirements();
    test_semantic_similarity_tautology_scores_near_zero();
    test_semantic_similarity_liveness_in_range();
    test_semantic_similarity_all_timings_in_range();
}
