#include <string>
#include <utility>
#include <vector>

#include "filter/implication.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement make_req(const std::string& ltl) {
    return Requirement(Formula("p"), Formula("q"), timing::immediately(), ltl);
}

Specification make_spec(const std::vector<std::string>& requirement_ltls) {
    std::vector<Requirement> reqs;
    reqs.reserve(requirement_ltls.size());
    for (const auto& ltl : requirement_ltls) {
        reqs.push_back(make_req(ltl));
    }
    return Specification({}, std::move(reqs), {}, {});
}

// --- make_implication_filter ---

void test_single_spec_returned_unchanged() {
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({"G a"})});
    expect(pop.size() == 1,
           "implication_filter: single spec should be returned unchanged");
}

void test_independent_specs_both_kept() {
    // G a and G b are incomparable: neither implies the other.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto pop = filter({make_spec({"G a"}), make_spec({"G b"})});
    expect(pop.size() == 2,
           "implication_filter: incomparable specs should both be retained");
}

void test_dominated_spec_removed() {
    // G a -> F a (if a holds always, it holds eventually), but not vice versa.
    // So G a strictly dominates F a, and F a must be removed.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_strong = make_spec({"G a"});
    const auto spec_weak = make_spec({"F a"});
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
    const auto pop = filter({make_spec({"G a"}), make_spec({"G a"})});
    expect(pop.size() == 2,
           "implication_filter: equivalent specs should both be retained");
}

void test_chain_keeps_only_strongest() {
    // G a & G b  =>  G a  =>  F a  (strict chain)
    // Only the spec with both G a and G b is maximal.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_strong = make_spec({"G a", "G b"});
    const auto spec_mid = make_spec({"G a"});
    const auto spec_weak = make_spec({"F a"});
    const auto pop = filter({spec_strong, spec_mid, spec_weak});
    expect(pop.size() == 1,
           "implication_filter: chain should keep only the strongest spec");
    expect(pop[0].m_guarantees.size() == 2,
           "implication_filter: surviving spec should be the one with two "
           "requirements");
}

void test_mixed_population() {
    // A (G a & G b) strictly dominates B (G a) and C (F a).
    // D (G b) dominates C but not the others; A also dominates D.
    // D is dominated by A (A => D: G a & G b => G b). So only A survives.
    // Actually wait: does A strictly dominate D?
    // A = (G a) & (G b); D = (G b)
    // (G a & G b) & !(G b) = G a & G b & F !b — UNSAT. So A implies D.
    // (G b) & !(G a & G b) = G b & (F !a | F !b) = G b & F !a — SAT. D doesn't
    // imply A. So A strictly dominates D. Only A survives.
    SatisfiabilityChecker checker;
    FilterFunction filter = make_implication_filter(checker);
    const auto spec_a = make_spec({"G a", "G b"});
    const auto spec_b = make_spec({"G a"});
    const auto spec_c = make_spec({"F a"});
    const auto spec_d = make_spec({"G b"});
    const auto pop = filter({spec_a, spec_b, spec_c, spec_d});
    expect(pop.size() == 1,
           "implication_filter: mixed population should keep only spec with "
           "both G a and G b");
    expect(pop[0].m_guarantees.size() == 2,
           "implication_filter: surviving spec should have two requirements");
}

}  // namespace

void run_implication_filter_tests() {
    test_single_spec_returned_unchanged();
    test_independent_specs_both_kept();
    test_dominated_spec_removed();
    test_equivalent_specs_both_kept();
    test_chain_keeps_only_strongest();
    test_mixed_population();
}
