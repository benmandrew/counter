#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "fitness/syntactic_similarity.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement make_req(const std::string& trigger, const std::string& response) {
    return Requirement{Formula(trigger), Formula(response),
                       timing::immediately()};
}

Specification make_spec(
    std::initializer_list<std::pair<const char*, const char*>> reqs) {
    std::vector<Requirement> req_vec;
    req_vec.reserve(reqs.size());
    for (const auto& [t, r] : reqs) {
        req_vec.push_back(make_req(t, r));
    }
    return Specification({}, std::move(req_vec), {}, {});
}

// --- requirement-level ---

void test_req_similarity_averages_component_scores() {
    const Requirement requirement{Formula("P"), Formula("Q"),
                                  timing::immediately()};
    const Requirement other_requirement{Formula("P"), Formula("P|Q"),
                                        timing::immediately()};

    const double synsim = syntactic_similarity(requirement, other_requirement);
    expect(std::fabs(synsim - (8.0 / 9.0)) < 1e-12,
           "syntactic-similarity: component averaging should produce the "
           "expected score for 'P'/'Q' versus 'P'/'P|Q'");
}

// --- specification-level ---

void test_spec_similarity_identical_single_req() {
    // All components identical → 1.0
    const Specification spec = make_spec({{"p", "q"}});
    const double result = syntactic_similarity(spec, spec);
    expect(std::fabs(result - 1.0) < 1e-12,
           "spec-similarity: identical single-req specs should score 1.0");
}

void test_spec_similarity_disjoint_atoms() {
    // Triggers share no atoms, responses share no atoms.
    // trigger_sim = 0, response_sim = 0, timing = 1 → (0+0+1)/3 = 1/3
    const Specification spec_a = make_spec({{"p", "q"}});
    const Specification spec_b = make_spec({{"r", "s"}});
    const double result = syntactic_similarity(spec_a, spec_b);
    expect(std::fabs(result - (1.0 / 3.0)) < 1e-12,
           "spec-similarity: fully disjoint single-req specs should score 1/3");
}

void test_spec_similarity_same_trigger_different_response() {
    // trigger_sim = 1, response_sim = 0, timing = 1 → (1+0+1)/3 = 2/3
    const Specification spec_a = make_spec({{"p", "q"}});
    const Specification spec_b = make_spec({{"p", "r"}});
    const double result = syntactic_similarity(spec_a, spec_b);
    expect(
        std::fabs(result - (2.0 / 3.0)) < 1e-12,
        "spec-similarity: same trigger, different response should score 2/3");
}

void test_spec_similarity_identical_multi_req() {
    // Identical two-requirement specs → 1.0
    const Specification spec = make_spec({{"p", "q"}, {"r", "s"}});
    const double result = syntactic_similarity(spec, spec);
    expect(std::fabs(result - 1.0) < 1e-12,
           "spec-similarity: identical multi-req specs should score 1.0");
}

void test_spec_similarity_partial_match_multi_req() {
    // Spec A = {req(p,q), req(r,s)} — triggers conjoin to (p & r), responses
    // to (q & s).
    // Spec B = {req(p,q), req(t,u)} — triggers conjoin to (p & t), responses
    // to (q & u).
    // (p & r) vs (p & t): 3 nodes each, 1 shared (atom p) → 0.5*(1/3+1/3) =
    // 1/3.
    // (q & s) vs (q & u): same shape → 1/3.
    // timing = 1 → (1/3 + 1/3 + 1) / 3 = (5/3) / 3 = 5/9.
    const Specification spec_a = make_spec({{"p", "q"}, {"r", "s"}});
    const Specification spec_b = make_spec({{"p", "q"}, {"t", "u"}});
    const double result = syntactic_similarity(spec_a, spec_b);
    expect(std::fabs(result - (5.0 / 9.0)) < 1e-12,
           "spec-similarity: specs sharing one of two requirements should "
           "score 5/9");
}

// --- timing similarity ---

// Identical timings always score 1.0.
void test_timing_identical_immediately() {
    const Requirement req{Formula("p"), Formula("q"), timing::immediately()};
    const double result = syntactic_similarity(req, req);
    // All three components equal 1.0 → average is 1.0
    expect(std::fabs(result - 1.0) < 1e-12,
           "timing-sim: identical requirements (immediately) should score 1.0");
}

void test_timing_identical_within_ticks() {
    const Requirement req{Formula("p"), Formula("p"), timing::within_ticks(3)};
    const double result = syntactic_similarity(req, req);
    expect(std::fabs(result - 1.0) < 1e-12,
           "timing-sim: identical within_ticks requirements should score 1.0");
}

// ForTicks{2} > ForTicks{1}: synSim = μ(↓ForTicks{1}) / μ(↓ForTicks{2}).
// With r=0.5, w=0.01:
//   μ(↓ForTicks{1}) = 3*0.01 + 0.5*(2-0.5)/0.5   = 0.03 + 1.5  = 1.53
//   μ(↓ForTicks{2}) = 3*0.01 + 0.5*(2-0.25)/0.5  = 0.03 + 1.75 = 1.78
//   synSim_timing = 1.53 / 1.78
// Both requirements share the same formulas (p/p), so formula components = 1.0.
// Overall = (1.0 + 1.0 + 1.53/1.78) / 3.0
void test_timing_comparable_for_ticks() {
    const Requirement req_strong{Formula("p"), Formula("p"),
                                 timing::for_ticks(2)};
    const Requirement req_weak{Formula("p"), Formula("p"),
                               timing::for_ticks(1)};
    const double timing_sim = 1.53 / 1.78;
    const double expected = (1.0 + 1.0 + timing_sim) / 3.0;
    const double result = syntactic_similarity(req_strong, req_weak);
    expect(std::fabs(result - expected) < 1e-9,
           "timing-sim: for_ticks{2} vs for_ticks{1} should give 1.53/1.78 "
           "timing component");
}

// ForTicks{1} vs Eventually: very different → timing component near 0.
// synSim_timing = μ(↓Eventually) / μ(↓ForTicks{1}) = 0.01 / 1.53
void test_timing_for_ticks_vs_eventually() {
    const Requirement req_strong{Formula("p"), Formula("p"),
                                 timing::for_ticks(1)};
    const Requirement req_weak{Formula("p"), Formula("p"),
                               timing::eventually()};
    const double timing_sim = 0.01 / 1.53;
    const double expected = (1.0 + 1.0 + timing_sim) / 3.0;
    const double result = syntactic_similarity(req_strong, req_weak);
    expect(std::fabs(result - expected) < 1e-9,
           "timing-sim: for_ticks{1} vs eventually should give tiny timing "
           "component");
}

// Immediately vs NextTimepoint are incomparable in the partial order.
// ↓I ∩ ↓N = WithinTicks{k≥1} ∪ {Eventually} → μ = 0.01 + 1.0 = 1.01
// μ(↓I) = μ(↓N) = 0.01 + 0.01 + 1.0 = 1.02
// μ(∪) = 1.02 + 1.02 - 1.01 = 1.03
// synSim_timing = 1.01 / 1.03
void test_timing_immediately_vs_next_timepoint() {
    const Requirement req_i{Formula("p"), Formula("p"), timing::immediately()};
    const Requirement req_n{Formula("p"), Formula("p"),
                            timing::next_timepoint()};
    const double timing_sim = 1.01 / 1.03;
    const double expected = (1.0 + 1.0 + timing_sim) / 3.0;
    const double result = syntactic_similarity(req_i, req_n);
    expect(std::fabs(result - expected) < 1e-9,
           "timing-sim: immediately vs next_timepoint should give 1.01/1.03 "
           "timing component");
}

}  // namespace

void run_syntactic_similarity_tests() {
    test_req_similarity_averages_component_scores();
    test_spec_similarity_identical_single_req();
    test_spec_similarity_disjoint_atoms();
    test_spec_similarity_same_trigger_different_response();
    test_spec_similarity_identical_multi_req();
    test_spec_similarity_partial_match_multi_req();
    test_timing_identical_immediately();
    test_timing_identical_within_ticks();
    test_timing_comparable_for_ticks();
    test_timing_for_ticks_vs_eventually();
    test_timing_immediately_vs_next_timepoint();
}
