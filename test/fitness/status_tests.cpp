#include <string>
#include <vector>

#include "fitness/status.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// Builds a one-requirement spec with an immediate trigger/response. The LTL
// is derived automatically from trigger/response/timing; these tests only
// exercise specification_status's propositional pre-checks (trigger/response
// satisfiability), which never look at the LTL string.
Specification make_spec(const std::string& trigger, const std::string& response,
                        const std::vector<std::string>& in_atoms = {},
                        const std::vector<std::string>& out_atoms = {}) {
    return Specification({},
                         {Requirement(Formula(trigger), Formula(response),
                                      timing::immediately())},
                         in_atoms, out_atoms);
}

// --- specification_status ---

void test_status_unsat_trigger_returns_zero() {
    // Condition p & !p is unsatisfiable, so is `condition & response`.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("p & !p", "q");
    expect(specification_status(spec, sat, real) ==
               k_status_component_unsatisfiable,
           "status: unsatisfiable condition should score the component tier");
}

void test_status_unsat_response_returns_zero() {
    // An unsatisfiable response makes `condition & response` unsatisfiable, so
    // the single component tier catches it without a check of its own.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("p", "q & !q");
    expect(specification_status(spec, sat, real) ==
               k_status_component_unsatisfiable,
           "status: unsatisfiable response should score the component tier");
}

void test_status_unsat_conjunction_returns_zero() {
    // Condition p and response !p are individually satisfiable but cannot hold
    // together, which is what makes the requirement incoherent.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("p", "!p");
    expect(specification_status(spec, sat, real) ==
               k_status_component_unsatisfiable,
           "status: a condition and response that cannot hold together should "
           "score the component tier");
}

void test_status_unrealizable_returns_point_five() {
    // Two guarantees whose propositional projections are all satisfiable but
    // whose combined LTL is unrealizable:
    //   G(true -> F o)  — output must eventually be true, infinitely often
    //   G(o -> i)       — whenever output is true, input must already be true
    // The environment plays i=false forever, so by G(o -> i) the controller
    // can never set o=true, yet the first guarantee demands it does so
    // infinitely often.
    //
    // Propositional checks pass:
    //   conj_a  = (true) & (o)       — SAT
    //   conj_g  = (o) & (i)          — SAT
    //   conj_ag = (true & o) & (o & i) = (o & i) — SAT
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {},
        {Requirement(Formula("true"), Formula("o"), timing::eventually()),
         Requirement(Formula("o"), Formula("i"), timing::immediately())},
        {"i"}, {"o"});
    expect(specification_status(spec, sat, real) == k_status_unrealizable,
           "status: satisfiable but unrealizable spec should score the "
           "unrealizable tier");
}

void test_status_jointly_unsat_responses_pass_individual_checks() {
    // Two requirements whose responses are individually satisfiable but
    // jointly contradictory. Components are per-requirement, so both pass and
    // the realizability call decides.
    // G(a -> b) & G(!a -> !b) = G(a <-> b) is realizable (set b := a).
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {},
        {Requirement(Formula("a"), Formula("b"), timing::immediately()),
         Requirement(Formula("!a"), Formula("!b"), timing::immediately())},
        {"a"}, {"b"});
    expect(specification_status(spec, sat, real) == k_status_realizable,
           "status: jointly unsat responses that are individually coherent "
           "should still reach the realizability tier");
}

void test_status_realizable_returns_one() {
    // G(i -> o): controller mirrors the input. Strategy o := i always works.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("i", "o", {"i"}, {"o"});
    expect(specification_status(spec, sat, real) == k_status_realizable,
           "status: satisfiable and realizable spec should score 1.0");
}

// `G o` as an assumption over the system's own output: the system satisfies
// the specification by holding o false, which breaks the assumption and makes
// the implication hold for nothing. Realizable, so the old three-point scale
// scored it 1.0 -- level with a genuine repair. It now scores level with
// unrealizable instead, which is the whole of the change: the search is paid
// for repairing, not for cheating.
void test_status_ill_separated_scores_level_with_unrealizable() {
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {Requirement(Formula("true"), Formula("o"), timing::always())},
        {Requirement(Formula("i"), Formula("o"), timing::immediately())}, {"i"},
        {"o"});
    expect(specification_status(spec, sat, real) == k_status_unrealizable,
           "status: a spec realizable only by defeating its own assumptions "
           "should score level with unrealizable, not with a genuine repair");
}

// The counterpart, so the tier above is not passing for the wrong reason: an
// assumption over an input atom alone cannot be defeated by the system, and
// still scores as a genuine repair.
void test_status_input_only_assumption_still_scores_one() {
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {Requirement(Formula("true"), Formula("i"), timing::always())},
        {Requirement(Formula("i"), Formula("o"), timing::immediately())}, {"i"},
        {"o"});
    expect(specification_status(spec, sat, real) == k_status_realizable,
           "status: a realizable spec whose assumption is over inputs alone "
           "should still score 1.0");
}

void test_status_no_guarantees_skips_the_solver() {
    // An empty guarantee side leaves a `true` consequent, so the score is
    // realizable without a solver call. RealizabilityChecker is left
    // default-constructed and unused, which is the point.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {Requirement(Formula("i"), Formula("i"), timing::immediately())}, {},
        {"i"}, {"o"});
    expect(specification_status(spec, sat, real) == k_status_realizable,
           "status: no guarantees should score realizable");
}

}  // namespace

void run_status_tests() {
    test_status_unsat_trigger_returns_zero();
    test_status_unsat_response_returns_zero();
    test_status_unsat_conjunction_returns_zero();
    test_status_jointly_unsat_responses_pass_individual_checks();
    test_status_unrealizable_returns_point_five();
    test_status_realizable_returns_one();
    test_status_ill_separated_scores_level_with_unrealizable();
    test_status_input_only_assumption_still_scores_one();
    test_status_no_guarantees_skips_the_solver();
}
