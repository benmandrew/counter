#include <array>
#include <chrono>
#include <optional>
#include <string>

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

// This codebase spells its boolean constants as atoms named "true"/"false",
// which black parses as free variables — invoked directly on any case below it
// answers SAT, including the four that are unsatisfiable. check_satisfiability
// gets them right two ways over: ltlfilt folds most to a constant before black
// is consulted, and anything reaching black has its constants rewritten to the
// "True"/"False" spelling black reads as constants.
void test_boolean_constants(const std::chrono::milliseconds& timeout) {
    struct Case {
        const char* formula;
        bool satisfiable;
    };
    const std::array<Case, 7> cases{{
        {"false", false},
        {"!true", false},
        {"G(false)", false},
        {"p & false", false},
        {"true", true},
        {"G(true)", true},
        {"p | true", true},
    }};
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    for (const Case& test_case : cases) {
        const std::optional<bool> result =
            checker.check_satisfiability(test_case.formula);
        if (!result.has_value()) {
            fail(std::string("black-runner: ") + test_case.formula +
                 " should be decided, not indeterminate");
        }
        expect(*result == test_case.satisfiable,
               std::string("black-runner: ") + test_case.formula +
                   " should be " +
                   (test_case.satisfiable ? "satisfiable" : "unsatisfiable"));
    }
}

// The constants are rewritten to black's "True"/"False" by whole token, so
// atoms that merely contain or abut them must survive untouched. Reading
// "true_count" as "True_count" would silently rename the variable; reading its
// prefix as a constant would corrupt the formula outright.
void test_constant_rewrite_respects_token_boundaries(
    const std::chrono::milliseconds& timeout) {
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    // "true_count" and "True_count" are distinct atoms, so asserting one and
    // negating the other is satisfiable. A substring replacement would rewrite
    // the former into the latter, collide them, and report UNSAT.
    const std::array<const char*, 4> formulae{{
        "true_count & !True_count",
        "is_false & !is_False",
        "falsey & X(!falsey)",
        "truth & X(!truth)",
    }};
    for (const char* formula : formulae) {
        const std::optional<bool> result =
            checker.check_satisfiability(formula);
        expect(result.has_value() && *result,
               std::string("black-runner: ") + formula +
                   " should be satisfiable — the atom must not be rewritten as "
                   "a boolean constant");
    }
}

// Regression: the implication check "(from) & !(dest)" reduces to false
// whenever from implies dest, and the genetic algorithm produces such pairs
// constantly with a vacuous G(true) conjunct. Here dest is from plus G(true),
// so from implies dest and the conjunction is unsatisfiable. Asked directly,
// black reads G(true) as a constraint on a free variable and reports SAT.
void test_implication_check_with_vacuous_conjunct(
    const std::chrono::milliseconds& timeout) {
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    const std::optional<bool> result = checker.check_satisfiability(
        "((G(a)) & (G(b))) & !(((G(a)) & (G(b))) & (G(true)))");
    expect(result.has_value() && !*result,
           "black-runner: implication check with a vacuous G(true) conjunct "
           "should be unsatisfiable");
}

}  // namespace

void run_black_runner_tests(const std::chrono::milliseconds& timeout) {
    test_satisfiable_simple(timeout);
    test_unsatisfiable_contradiction(timeout);
    test_satisfiable_ltl(timeout);
    test_unsatisfiable_ltl(timeout);
    test_boolean_constants(timeout);
    test_constant_rewrite_respects_token_boundaries(timeout);
    test_implication_check_with_vacuous_conjunct(timeout);
}
