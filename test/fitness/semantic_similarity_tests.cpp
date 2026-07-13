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
        semantic_similarity(requirement, other_requirement, Config{});
    const double with_explicit_step_count = semantic_similarity(
        requirement, other_requirement, Config{}.default_model_counting_bound);
    expect(std::fabs(with_default - with_explicit_step_count) < 1e-12,
           "semantic-similarity: default overload should use "
           "Config{}.default_model_counting_bound");
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

// Regression: the p_add_assumption mutation grows a candidate's assumption
// list, so a candidate can be scored against an original with fewer
// assumptions. The specification-level overload used to advance a second
// iterator in lockstep to the first spec's end, running it past the shorter
// spec's assumptions (an assertion in debug builds, undefined behaviour once
// NDEBUG disables it). The score must stay finite, bounded, and independent of
// argument order.
void test_semantic_similarity_differing_assumption_counts() {
    const Requirement req_q{Formula("P"), Formula("Q"), timing::immediately()};
    const Requirement req_not_q{Formula("P"), Formula("!Q"),
                                timing::immediately()};
    const Specification original({req_q}, {req_q}, {"P"}, {"Q"});
    const Specification candidate({req_not_q, req_q}, {req_q}, {"P"}, {"Q"});
    const double candidate_vs_original =
        semantic_similarity(candidate, original, 1);
    const double original_vs_candidate =
        semantic_similarity(original, candidate, 1);
    expect(std::isfinite(candidate_vs_original) &&
               candidate_vs_original >= 0.0 && candidate_vs_original <= 1.0,
           "semantic-similarity: a candidate with an extra assumption must "
           "score a finite, bounded value against the original (no "
           "out-of-bounds read)");
    expect(std::fabs(candidate_vs_original - original_vs_candidate) < 1e-12,
           "semantic-similarity: differing assumption counts score the same in "
           "either argument order");
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

// Propositionally-equivalent responses that differ syntactically must produce
// the same semantic similarity when scored against the same original.
// Regression: repair_2 and repair_13 from the takeoff-unfixed-1 run had
// divergent stored scores because the automata happened to differ in an older
// version of requirement_to_ltl; the current code must agree.
void test_semantic_similarity_propequiv_responses_score_equal() {
    // repair_2.req2: response = !((!tr & lo) -> tr)  ≡  !tr & lo
    const Requirement repair2_req2{
        Formula("true"),
        Formula("!(((!(takeoff_roll)) & (lift_off)) -> (takeoff_roll))"),
        timing::within_ticks(5), ConditionType::Trigger};
    // repair_13.req2: response = !tr & lo  (same formula, simpler string)
    const Requirement repair13_req2{
        Formula("true"), Formula("(!(takeoff_roll)) & (lift_off)"),
        timing::within_ticks(5), ConditionType::Trigger};
    // original req2
    const Requirement original_req2{
        Formula("true"), Formula("!(takeoff_roll) & (lift_off)"),
        timing::within_ticks(7), ConditionType::Trigger};
    const double score2 = semantic_similarity(
        repair2_req2, original_req2, Config{}.default_model_counting_bound);
    const double score13 = semantic_similarity(
        repair13_req2, original_req2, Config{}.default_model_counting_bound);
    expect(std::fabs(score2 - score13) < 1e-12,
           "semantic-similarity: propositionally-equivalent responses must "
           "score identically against the same original (got " +
               std::to_string(score2) + " vs " + std::to_string(score13) + ")");
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
    test_semantic_similarity_differing_assumption_counts();
    test_semantic_similarity_specification_averages_requirements();
    test_semantic_similarity_tautology_scores_near_zero();
    test_semantic_similarity_liveness_in_range();
    test_semantic_similarity_all_timings_in_range();
    test_semantic_similarity_propequiv_responses_score_equal();
}
