#include <string>
#include <utility>
#include <vector>

#include "filter/implication.hpp"
#include "filter/implication_check.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// "G a": a holds at every timepoint.
Requirement g_req(const std::string& atom) {
    return Requirement(Formula("true"), Formula(atom), timing::immediately());
}

// "G(true -> F a)", i.e. "GF a": a holds infinitely often. Strictly weaker
// than g_req(a) (G a implies GF a, but not vice versa), used as the "weak"
// end of the dominance chain in these tests.
Requirement f_req(const std::string& atom) {
    return Requirement(Formula("true"), Formula(atom), timing::eventually());
}

Specification make_spec(std::vector<Requirement> reqs) {
    return Specification({}, std::move(reqs), {}, {});
}

// --- make_implication_filter ---

void test_single_spec_returned_unchanged() {
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")})});
    expect(pop.size() == 1,
           "implication_filter: single spec should be returned unchanged");
}

void test_independent_specs_both_kept() {
    // G a and G b are incomparable: neither implies the other.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")}), make_spec({g_req("b")})});
    expect(pop.size() == 2,
           "implication_filter: incomparable specs should both be retained");
}

void test_dominated_spec_removed() {
    // G a -> GF a (if a holds always, it holds infinitely often), but not
    // vice versa. So G a strictly dominates GF a, and GF a must be removed.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_strong = make_spec({g_req("a")});
    const auto spec_weak = make_spec({f_req("a")});
    const auto pop = filter({spec_strong, spec_weak});
    expect(pop.size() == 1,
           "implication_filter: dominated spec should be removed");
    expect(pop[0].m_guarantees[0].m_ltl == spec_strong.m_guarantees[0].m_ltl,
           "implication_filter: the stronger spec (G a) should survive");
}

void test_equivalent_specs_both_kept() {
    // Two specs with identical LTL strings imply each other; neither strictly
    // dominates the other, so both must be retained.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")}), make_spec({g_req("a")})});
    expect(pop.size() == 2,
           "implication_filter: equivalent specs should both be retained");
}

void test_chain_keeps_only_strongest() {
    // G a & G b  =>  G a  =>  GF a  (strict chain)
    // Only the spec with both G a and G b is maximal.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_strong = make_spec({g_req("a"), g_req("b")});
    const auto spec_mid = make_spec({g_req("a")});
    const auto spec_weak = make_spec({f_req("a")});
    const auto pop = filter({spec_strong, spec_mid, spec_weak});
    expect(pop.size() == 1,
           "implication_filter: chain should keep only the strongest spec");
    expect(pop[0].m_guarantees.size() == 2,
           "implication_filter: surviving spec should be the one with two "
           "requirements");
}

void test_mixed_population() {
    // A (G a & G b) strictly dominates B (G a) and C (GF a).
    // A also strictly dominates D (G b): (G a & G b) & !(G b) is UNSAT, but
    // (G b) & !(G a & G b) is SAT (b always true, a not), so D does not
    // imply A. Only A survives.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_a = make_spec({g_req("a"), g_req("b")});
    const auto spec_b = make_spec({g_req("a")});
    const auto spec_c = make_spec({f_req("a")});
    const auto spec_d = make_spec({g_req("b")});
    const auto pop = filter({spec_a, spec_b, spec_c, spec_d});
    expect(pop.size() == 1,
           "implication_filter: mixed population should keep only spec with "
           "both G a and G b");
    expect(pop[0].m_guarantees.size() == 2,
           "implication_filter: surviving spec should have two requirements");
}

}  // namespace

// --- spec_implies propositional shortcut ---
// These tests cover the propositional-response shortcut in requirement_implies:
// when two requirements share the same condition, timing, and condition_type,
// implication reduces to a propositional check on the responses alone, avoiding
// the expensive temporal LTL check that can time out under concurrent load.

void test_weakening_response_implies() {
    // A weaker (dropped-conjunct) response on the same condition/timing
    // must be recognised as implied by the original: the propositional shortcut
    // should confirm that (!a & b) -> b without needing a temporal LTL check.
    SatisfiabilityChecker checker;
    const Specification original(
        {},
        {Requirement(Formula("true"), Formula("!a & b"),
                     timing::within_ticks(5))},
        {}, {});
    const Specification candidate(
        {},
        {Requirement(Formula("true"), Formula("b"), timing::within_ticks(5))},
        {}, {});
    expect(spec_implies(original, candidate, checker).value_or(false),
           "spec_implies: weaker response (b) implied by (!a & b)");
    expect(!spec_implies(candidate, original, checker).value_or(true),
           "spec_implies: stronger response (!a & b) not implied by (b)");
}

void test_independent_responses_not_implied() {
    // Two requirements with unrelated responses: neither implies the other.
    SatisfiabilityChecker checker;
    const Specification spec_a(
        {},
        {Requirement(Formula("true"), Formula("a"), timing::within_ticks(5))},
        {}, {});
    const Specification spec_b(
        {},
        {Requirement(Formula("true"), Formula("b"), timing::within_ticks(5))},
        {}, {});
    expect(!spec_implies(spec_a, spec_b, checker).value_or(true),
           "spec_implies: unrelated responses should not imply each other (a)");
    expect(!spec_implies(spec_b, spec_a, checker).value_or(true),
           "spec_implies: unrelated responses should not imply each other (b)");
}

void run_implication_filter_tests() {
    test_single_spec_returned_unchanged();
    test_independent_specs_both_kept();
    test_dominated_spec_removed();
    test_equivalent_specs_both_kept();
    test_chain_keeps_only_strongest();
    test_mixed_population();
    test_weakening_response_implies();
    test_independent_responses_not_implied();
}
