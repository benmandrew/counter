#include <chrono>
#include <optional>

#include "runner/black.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_satisfiable_simple(const std::chrono::milliseconds& timeout) {
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    const std::optional<bool> result = checker.check_satisfiability("F p");
    expect(result.has_value() && *result,
           "black-runner: F p should be satisfiable");
}

void test_unsatisfiable_contradiction(
    const std::chrono::milliseconds& timeout) {
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    const std::optional<bool> result = checker.check_satisfiability("p & !p");
    expect(result.has_value() && !*result,
           "black-runner: p & !p should be unsatisfiable");
}

void test_satisfiable_ltl(const std::chrono::milliseconds& timeout) {
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    const std::optional<bool> result = checker.check_satisfiability("G F p");
    expect(result.has_value() && *result,
           "black-runner: G F p should be satisfiable");
}

void test_unsatisfiable_ltl(const std::chrono::milliseconds& timeout) {
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    const std::optional<bool> result =
        checker.check_satisfiability("G !p & F p");
    expect(result.has_value() && !*result,
           "black-runner: G !p & F p should be unsatisfiable");
}

}  // namespace

void run_black_runner_tests(const std::chrono::milliseconds& timeout) {
    test_satisfiable_simple(timeout);
    test_unsatisfiable_contradiction(timeout);
    test_satisfiable_ltl(timeout);
    test_unsatisfiable_ltl(timeout);
}
