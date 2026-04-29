#include <string>
#include <utility>
#include <vector>

#include "requirement.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement make_req(const std::string& trigger, const std::string& response,
                     Timing timing, LtlSpec spec) {
    Requirement req{Formula(trigger), Formula(response), timing};
    req.m_ltl = std::move(spec);
    return req;
}

void test_realizable_immediately() {
    expect(check_realizability(make_req("P", "Q", timing::immediately(),
                                        {"G((P) -> (Q))", {"P"}, {"Q"}})),
           "spot-runner: G(P -> Q) should be realisable");
}

void test_unrealizable_immediately() {
    expect(!check_realizability(make_req("true", "false", timing::immediately(),
                                         {"G((true) -> (false))", {}, {}})),
           "spot-runner: G(true -> false) should be unrealisable");
}

void test_realizable_next_timepoint() {
    expect(check_realizability(make_req("P", "Q", timing::next_timepoint(),
                                        {"G((P) -> (X(Q)))", {"P"}, {"Q"}})),
           "spot-runner: G(P -> XQ) should be realisable");
}

void test_realizable_within_ticks() {
    expect(
        check_realizability(make_req("P", "Q", timing::within_ticks(2),
                                     {"G((P) -> ((Q) | X(Q)))", {"P"}, {"Q"}})),
        "spot-runner: G(P -> Q | XQ) should be realisable");
}

void test_realizable_for_ticks() {
    expect(
        check_realizability(make_req("P", "Q", timing::for_ticks(2),
                                     {"G((P) -> ((Q) & X(Q)))", {"P"}, {"Q"}})),
        "spot-runner: G(P -> Q & XQ) should be realisable");
}

}  // namespace

void run_spot_runner_tests() {
    test_realizable_immediately();
    test_unrealizable_immediately();
    test_realizable_next_timepoint();
    test_realizable_within_ticks();
    test_realizable_for_ticks();
}
