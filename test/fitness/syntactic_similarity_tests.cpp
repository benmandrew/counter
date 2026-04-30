#include <cmath>
#include <set>
#include <string>
#include <utility>

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
    std::set<Requirement> req_set;
    for (const auto& [t, r] : reqs) {
        req_set.insert(make_req(t, r));
    }
    return Specification(std::move(req_set), {}, {});
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
    const Specification a = make_spec({{"p", "q"}});
    const double result = syntactic_similarity(a, a);
    expect(std::fabs(result - 1.0) < 1e-12,
           "spec-similarity: identical single-req specs should score 1.0");
}

void test_spec_similarity_disjoint_atoms() {
    // Triggers share no atoms, responses share no atoms.
    // trigger_sim = 0, response_sim = 0, timing = 1 → (0+0+1)/3 = 1/3
    const Specification a = make_spec({{"p", "q"}});
    const Specification b = make_spec({{"r", "s"}});
    const double result = syntactic_similarity(a, b);
    expect(std::fabs(result - (1.0 / 3.0)) < 1e-12,
           "spec-similarity: fully disjoint single-req specs should score 1/3");
}

void test_spec_similarity_same_trigger_different_response() {
    // trigger_sim = 1, response_sim = 0, timing = 1 → (1+0+1)/3 = 2/3
    const Specification a = make_spec({{"p", "q"}});
    const Specification b = make_spec({{"p", "r"}});
    const double result = syntactic_similarity(a, b);
    expect(
        std::fabs(result - (2.0 / 3.0)) < 1e-12,
        "spec-similarity: same trigger, different response should score 2/3");
}

void test_spec_similarity_identical_multi_req() {
    // Identical two-requirement specs → 1.0
    const Specification a = make_spec({{"p", "q"}, {"r", "s"}});
    const double result = syntactic_similarity(a, a);
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
    const Specification a = make_spec({{"p", "q"}, {"r", "s"}});
    const Specification b = make_spec({{"p", "q"}, {"t", "u"}});
    const double result = syntactic_similarity(a, b);
    expect(std::fabs(result - (5.0 / 9.0)) < 1e-12,
           "spec-similarity: specs sharing one of two requirements should "
           "score 5/9");
}

}  // namespace

void run_syntactic_similarity_tests() {
    test_req_similarity_averages_component_scores();
    test_spec_similarity_identical_single_req();
    test_spec_similarity_disjoint_atoms();
    test_spec_similarity_same_trigger_different_response();
    test_spec_similarity_identical_multi_req();
    test_spec_similarity_partial_match_multi_req();
}
