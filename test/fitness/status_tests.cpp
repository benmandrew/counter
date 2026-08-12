#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "config.hpp"
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

// --- status_score_mrs, against a stub oracle ---

// Records every subset the walk asked about, so the tests can pin the query
// sequence rather than only the resulting score.
struct RecordingOracle {
    std::vector<std::vector<std::size_t>> queries;
    std::function<bool(const std::vector<std::size_t>&)> admissible;

    bool operator()(const std::vector<std::size_t>& indices) {
        queries.push_back(indices);
        return admissible(indices);
    }
};

void test_mrs_keeps_everything_when_all_parts_are_admissible() {
    SatisfiabilityChecker sat;
    RecordingOracle oracle{
        {}, [](const std::vector<std::size_t>&) { return true; }};
    const double score = status_score_mrs(
        {"p"}, 4, sat,
        [&oracle](const std::vector<std::size_t>& idx) { return oracle(idx); });
    expect(score == k_status_realizable,
           "mrs: a fully realizable guarantee side should score exactly 1.0");
    expect(oracle.queries.size() == 4,
           "mrs: the walk should ask once per part");
}

void test_mrs_scores_the_kept_fraction() {
    // Part 2 conflicts with part 0, so the walk keeps 0, 1 and 3.
    SatisfiabilityChecker sat;
    const double score = status_score_mrs(
        {"p"}, 4, sat, [](const std::vector<std::size_t>& indices) {
            const bool has_zero =
                std::find(indices.begin(), indices.end(), 0) != indices.end();
            const bool has_two =
                std::find(indices.begin(), indices.end(), 2) != indices.end();
            return !(has_zero && has_two);
        });
    expect(score == 0.75, "mrs: three parts of four kept should score 0.75");
}

void test_mrs_walk_carries_only_the_accepted_prefix() {
    // A rejected part is dropped rather than carried, so every later query is
    // about a subset the walk has actually accepted. That is what makes the
    // queries recur across near-identical candidates and hit the cache.
    SatisfiabilityChecker sat;
    RecordingOracle oracle{{}, [](const std::vector<std::size_t>& indices) {
                               return std::find(indices.begin(), indices.end(),
                                                1) == indices.end();
                           }};
    const double score = status_score_mrs(
        {"p"}, 3, sat,
        [&oracle](const std::vector<std::size_t>& idx) { return oracle(idx); });
    expect(score == 2.0 / 3.0, "mrs: rejecting one part of three scores 2/3");
    const std::vector<std::vector<std::size_t>> expected = {
        {0}, {0, 1}, {0, 2}};
    expect(oracle.queries == expected,
           "mrs: a rejected part should not appear in any later query");
}

void test_mrs_walk_is_deterministic() {
    // Parts 0 and 1 conflict, and whichever comes first is kept: greedy returns
    // a maximal subset, not a maximum one. Pinned so the property is not taken
    // for a bug later, and so the score stays a deterministic function of the
    // candidate, which seed reproducibility requires.
    SatisfiabilityChecker sat;
    const auto conflicting = [](const std::vector<std::size_t>& indices) {
        return indices.size() < 2;
    };
    const double first = status_score_mrs({"p"}, 2, sat, conflicting);
    const double second = status_score_mrs({"p"}, 2, sat, conflicting);
    expect(first == 0.5 && first == second,
           "mrs: the greedy walk should be deterministic across calls");
}

void test_mrs_short_circuits_on_an_unsatisfiable_component() {
    // The component tier still runs first, and costs satisfiability queries
    // rather than the n realizability queries the walk would otherwise pay to
    // learn nothing.
    SatisfiabilityChecker sat;
    RecordingOracle oracle{
        {}, [](const std::vector<std::size_t>&) { return true; }};
    const double score = status_score_mrs(
        {"p & !p"}, 3, sat,
        [&oracle](const std::vector<std::size_t>& idx) { return oracle(idx); });
    expect(score == k_status_component_unsatisfiable,
           "mrs: an unsatisfiable component should score the component tier");
    expect(oracle.queries.empty(),
           "mrs: the component tier should short-circuit the walk entirely");
}

void test_mrs_empty_guarantee_side_scores_realizable() {
    SatisfiabilityChecker sat;
    const double score =
        status_score_mrs({"p"}, 0, sat, [](const std::vector<std::size_t>&) {
            fail("mrs: an empty guarantee side should not query the oracle");
            return false;
        });
    expect(score == k_status_realizable,
           "mrs: no guarantee-side parts should score realizable");
}

// --- specification_status under StatusGrading::Mrs ---

void test_mrs_grades_an_unrealizable_spec_between_the_tiers() {
    // Three guarantees, of which the first two are jointly realizable and the
    // third breaks them:
    //   G(i -> o)  -- mirror the input
    //   G F o      -- o must hold infinitely often
    //   G(o -> i)  -- o only where i already holds
    // The environment plays i false forever, so the third forbids o and the
    // second demands it. Greedy keeps the first two, scoring 2/3 -- a value the
    // tiered scale cannot express, on a spec it scores 0.5.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {},
        {Requirement(Formula("i"), Formula("o"), timing::immediately()),
         Requirement(Formula("true"), Formula("o"), timing::eventually()),
         Requirement(Formula("o"), Formula("i"), timing::immediately())},
        {"i"}, {"o"});
    expect(specification_status(spec, sat, real) == k_status_unrealizable,
           "mrs: the tiered scale should score this spec 0.5");
    expect(
        specification_status(spec, sat, real, StatusGrading::Mrs) == 2.0 / 3.0,
        "mrs: two of three guarantees kept should score 2/3");
}

void test_mrs_realizable_spec_still_scores_one() {
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("i", "o", {"i"}, {"o"});
    expect(specification_status(spec, sat, real, StatusGrading::Mrs) ==
               k_status_realizable,
           "mrs: a realizable spec should still score exactly 1.0");
}

void test_mrs_ill_separated_spec_does_not_score_one() {
    // Well-separation is folded into the subset oracle, so a guarantee only
    // reachable by defeating the specification's own assumption is rejected
    // rather than kept. 1.0 keeps meaning what it means on the tiered scale:
    // realizable for a real reason.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {Requirement(Formula("true"), Formula("o"), timing::always())},
        {Requirement(Formula("i"), Formula("o"), timing::immediately())}, {"i"},
        {"o"});
    expect(specification_status(spec, sat, real, StatusGrading::Mrs) <
               k_status_realizable,
           "mrs: a spec realizable only by defeating its own assumptions must "
           "not score 1.0");
}

void test_mrs_input_only_assumption_still_scores_one() {
    // The counterpart, so the test above is not passing for the wrong reason.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec(
        {Requirement(Formula("true"), Formula("i"), timing::always())},
        {Requirement(Formula("i"), Formula("o"), timing::immediately())}, {"i"},
        {"o"});
    expect(specification_status(spec, sat, real, StatusGrading::Mrs) ==
               k_status_realizable,
           "mrs: an assumption over inputs alone should still score 1.0");
}

void test_mrs_defaults_to_the_tiered_scale() {
    // The default argument matters: every existing caller that passes no
    // grading must keep the behaviour it had.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("i", "o", {"i"}, {"o"});
    expect(specification_status(spec, sat, real) ==
               specification_status(spec, sat, real, StatusGrading::Tiered),
           "mrs: the default grading should be the tiered scale");
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
    test_mrs_keeps_everything_when_all_parts_are_admissible();
    test_mrs_scores_the_kept_fraction();
    test_mrs_walk_carries_only_the_accepted_prefix();
    test_mrs_walk_is_deterministic();
    test_mrs_short_circuits_on_an_unsatisfiable_component();
    test_mrs_empty_guarantee_side_scores_realizable();
    test_mrs_grades_an_unrealizable_spec_between_the_tiers();
    test_mrs_realizable_spec_still_scores_one();
    test_mrs_ill_separated_spec_does_not_score_one();
    test_mrs_input_only_assumption_still_scores_one();
    test_mrs_defaults_to_the_tiered_scale();
}
