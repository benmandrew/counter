#include <string>
#include <utility>
#include <vector>

#include "requirement.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement make_req(const std::string& trigger, const std::string& response,
                     Timing timing, std::string spec) {
    Requirement req{Formula(trigger), Formula(response), timing};
    req.m_ltl = std::move(spec);
    return req;
}

void test_realizable_immediately() {
    RealizabilityChecker checker;
    Requirement req =
        make_req("p", "q", timing::immediately(), "G((p) -> (q))");
    expect(checker.check_realizability(Specification({}, {req}, {"p"}, {"q"})),
           "spot-runner: G(p -> q) should be realisable");
}

void test_unrealizable_immediately() {
    RealizabilityChecker checker;
    Requirement req = make_req("true", "false", timing::immediately(),
                               "G((true) -> (false))");
    expect(!checker.check_realizability(Specification({}, {req}, {}, {})),
           "spot-runner: G(true -> false) should be unrealisable");
}

void test_realizable_next_timepoint() {
    RealizabilityChecker checker;
    Requirement req =
        make_req("p", "q", timing::next_timepoint(), "G((p) -> (X(q)))");
    expect(checker.check_realizability(Specification({}, {req}, {"p"}, {"q"})),
           "spot-runner: G(p -> Xq) should be realisable");
}

void test_realizable_within_ticks() {
    RealizabilityChecker checker;
    Requirement req =
        make_req("p", "q", timing::within_ticks(2), "G((p) -> ((q) | X(q)))");
    expect(checker.check_realizability(Specification({}, {req}, {"p"}, {"q"})),
           "spot-runner: G(p -> q | Xq) should be realisable");
}

void test_realizable_for_ticks() {
    RealizabilityChecker checker;
    Requirement req =
        make_req("p", "q", timing::for_ticks(2), "G((p) -> ((q) & X(q)))");
    expect(checker.check_realizability(Specification({}, {req}, {"p"}, {"q"})),
           "spot-runner: G(p -> q & Xq) should be realisable");
}

void test_realizable_eventually() {
    RealizabilityChecker checker;
    Requirement req =
        make_req("p", "q", timing::eventually(), "G((p) -> (F(q)))");
    expect(checker.check_realizability(Specification({}, {req}, {"p"}, {"q"})),
           "spot-runner: G(p -> Fq) should be realisable");
}

void test_realizable_with_assumption() {
    RealizabilityChecker checker;
    // Assumption: p persists once set (environment constraint)
    Requirement assumption =
        make_req("p", "p", timing::next_timepoint(), "G((p) -> (X(p)))");
    // Guarantee: when p holds, q must hold
    Requirement guarantee =
        make_req("p", "q", timing::immediately(), "G((p) -> (q))");
    expect(checker.check_realizability(
               Specification({assumption}, {guarantee}, {"p"}, {"q"})),
           "spot-runner: G(p->Xp) -> G(p->q) should be realizable");
}

void test_assumption_enables_joint_realizability() {
    RealizabilityChecker checker;
    // These two guarantees are jointly unrealizable on their own
    Requirement g1 =
        make_req("a", "b", timing::next_timepoint(), "G((a) -> (X(b)))");
    Requirement g2 =
        make_req("a", "!b", timing::next_timepoint(), "G((a) -> (X(!b)))");
    expect(
        !checker.check_realizability(Specification({}, {g1, g2}, {"a"}, {"b"})),
        "spot-runner: G(a->Xb) & G(a->X!b) should be unrealizable without "
        "assumption");
    // Assumption G(!a): environment never sets a — makes both guarantees
    // vacuously true
    Requirement assum =
        make_req("true", "!a", timing::immediately(), "G((true) -> (!a))");
    expect(checker.check_realizability(
               Specification({assum}, {g1, g2}, {"a"}, {"b"})),
           "spot-runner: G(!a) -> (G(a->Xb) & G(a->X!b)) should be realizable");
}

void test_individually_realizable_but_jointly_unrealizable() {
    RealizabilityChecker checker;
    // Req1: G(a -> X b)
    Requirement req1 =
        make_req("a", "b", timing::next_timepoint(), "G((a) -> (X(b)))");
    // Req2: G(a -> X !b)
    Requirement req2 =
        make_req("a", "!b", timing::next_timepoint(), "G((a) -> (X(!b)))");

    // Each is realizable alone
    expect(checker.check_realizability(Specification({}, {req1}, {"a"}, {"b"})),
           "spot-runner: G(a -> X b) should be realizable");
    expect(checker.check_realizability(Specification({}, {req2}, {"a"}, {"b"})),
           "spot-runner: G(a -> X !b) should be realizable");
    // Together, not realizable
    expect(!checker.check_realizability(
               Specification({}, {req1, req2}, {"a"}, {"b"})),
           "spot-runner: G(a -> X b) & G(a -> X !b) should be unrealizable");
}

}  // namespace

void run_spot_runner_tests() {
    test_realizable_eventually();
    test_realizable_immediately();
    test_unrealizable_immediately();
    test_realizable_next_timepoint();
    test_realizable_within_ticks();
    test_realizable_for_ticks();
    test_realizable_with_assumption();
    test_assumption_enables_joint_realizability();
    test_individually_realizable_but_jointly_unrealizable();
}
