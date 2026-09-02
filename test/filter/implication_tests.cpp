#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
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

void test_equivalent_specs_collapse_to_one() {
    // Two specs with identical LTL strings imply each other. They are one
    // repair written twice, so exactly one survives.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({g_req("a")}), make_spec({g_req("a")})});
    expect(pop.size() == 1,
           "implication_filter: equivalent specs should collapse to one");
}

void test_equivalence_tie_break_prefers_similar() {
    // "G(true -> a & a)" and "G(true -> a)" are logically equivalent and
    // structurally distinct, so the survivor is decided by the tie-break
    // rather than by duplicate collapsing. The original is "G(true -> a)", so
    // the second is the more similar and must be the one kept -- and it must
    // win from either input position, the tie-break being a property of the
    // specs rather than of the order the sweep happens to visit them in.
    SatisfiabilityChecker checker;
    const Specification original = make_spec({g_req("a")});
    const Config cfg;
    for (const bool similar_first : {false, true}) {
        FilterFunction filter = make_implication_filter(
            checker, syntactic_similarity_key(original, cfg));
        const Specification bulky = make_spec({g_req("a & a")});
        const Specification lean = make_spec({g_req("a")});
        const auto pop =
            similar_first ? filter({lean, bulky}) : filter({bulky, lean});
        expect(pop.size() == 1,
               "implication_filter: equivalent specs should collapse to one");
        expect(pop.size() == 1 && pop[0] == lean,
               "implication_filter: the survivor should be the spec closest to "
               "the original");
    }
}

void test_equivalence_without_key_still_collapses() {
    // With no similarity key the tie-break falls through to operator<, which
    // is what the `maximal` tool relies on: it has no original to rank
    // against, and must still not report one repair twice.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop =
        filter({make_spec({g_req("a & a")}), make_spec({g_req("a")})});
    expect(pop.size() == 1,
           "implication_filter: equivalent specs should collapse with no "
           "similarity key");
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
    test_equivalent_specs_collapse_to_one();
    test_equivalence_tie_break_prefers_similar();
    test_equivalence_without_key_still_collapses();
    test_chain_keeps_only_strongest();
    test_mixed_population();
    test_weakening_response_implies();
    test_independent_responses_not_implied();
}
