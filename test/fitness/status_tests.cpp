#include <string>
#include <vector>

#include "fitness/status.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// Builds a one-requirement spec with explicit trigger/response strings and an
// LTL string. The LTL string is used by the realizability checker; the trigger
// and response strings are used by the satisfiability checks inside
// specification_status.
Specification make_spec(const std::string& trigger, const std::string& response,
                        const std::string& ltl,
                        const std::vector<std::string>& in_atoms = {},
                        const std::vector<std::string>& out_atoms = {}) {
    return Specification({},
                         {Requirement(Formula(trigger), Formula(response),
                                      timing::immediately(), ltl)},
                         in_atoms, out_atoms);
}

// --- specification_status ---

void test_status_unsat_trigger_returns_zero() {
    // Trigger p & !p is unsatisfiable → score 0.0.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("p & !p", "q", "G(true)");
    expect(specification_status(spec, sat, real) == 0.0,
           "status: unsatisfiable trigger conjunction should score 0.0");
}

void test_status_unsat_response_returns_point_one() {
    // Trigger p is satisfiable; response q & !q is not → score 0.1.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("p", "q & !q", "G(true)");
    expect(specification_status(spec, sat, real) == 0.1,
           "status: unsatisfiable response conjunction should score 0.1");
}

void test_status_unsat_conjunction_returns_point_two() {
    // Trigger p and response !p are individually satisfiable, but p & !p is
    // not → score 0.2.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("p", "!p", "G(true)");
    expect(specification_status(spec, sat, real) == 0.2,
           "status: satisfiable trigger and response but unsatisfiable "
           "conjunction should score 0.2");
}

void test_status_unrealizable_returns_point_five() {
    // Two guarantees whose propositional projections are all satisfiable but
    // whose combined LTL is unrealizable:
    //   F o        — output must eventually be true
    //   G(o -> i)  — whenever output is true, input must already be true
    // The environment plays i=false forever, so the controller can never set
    // o=true (it would violate G(o -> i)) yet F o demands it eventually does.
    //
    // Propositional checks pass:
    //   conj_a  = (true) & (o)       — SAT
    //   conj_g  = (o) & (i)          — SAT
    //   conj_ag = (true & o) & (o & i) = (o & i) — SAT
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const Specification spec({},
                             {Requirement(Formula("true"), Formula("o"),
                                          timing::eventually(), "F o"),
                              Requirement(Formula("o"), Formula("i"),
                                          timing::immediately(), "G(o -> i)")},
                             {"i"}, {"o"});
    expect(specification_status(spec, sat, real) == 0.5,
           "status: satisfiable but unrealizable spec should score 0.5");
}

void test_status_realizable_returns_one() {
    // G(i -> o): controller mirrors the input. Strategy o := i always works.
    SatisfiabilityChecker sat;
    RealizabilityChecker real;
    const auto spec = make_spec("i", "o", "G(i -> o)", {"i"}, {"o"});
    expect(specification_status(spec, sat, real) == 1.0,
           "status: satisfiable and realizable spec should score 1.0");
}

}  // namespace

void run_status_tests() {
    test_status_unsat_trigger_returns_zero();
    test_status_unsat_response_returns_point_one();
    test_status_unsat_conjunction_returns_point_two();
    test_status_unrealizable_returns_point_five();
    test_status_realizable_returns_one();
}
