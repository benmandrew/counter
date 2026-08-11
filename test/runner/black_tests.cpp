#include <array>
#include <chrono>
#include <optional>
#include <string>

#include "runner/black.hpp"
#include "runner/ltlfilt.hpp"
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
    const std::array<Case, 11> cases{{
        {"false", false},
        {"!true", false},
        {"G(false)", false},
        {"p & false", false},
        {"true", true},
        {"G(true)", true},
        {"p | true", true},
        // SPOT's own spellings. These reach check_satisfiability whenever a
        // query has been through ltlfilt, which prints constants as 0/1 rather
        // than as the atoms this codebase writes. black rejects them as a
        // syntax error rather than misreading them, so before they were
        // handled a folded query aborted the whole run.
        {"0", false},
        {"G(0)", false},
        {"1", true},
        {"p | 1", true},
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
    const std::array<const char*, 7> formulae{{
        "true_count & !True_count",
        "is_false & !is_False",
        "falsey & X(!falsey)",
        "truth & X(!truth)",
        // The digit spellings need the same discipline, and the corpus is full
        // of atoms that end in one — "g_1" and "r_1" are the arbiter naming
        // convention. A substring rewrite would turn "g_1" into "g_True",
        // collapsing it onto the distinct atom of that name and reporting
        // UNSAT for a satisfiable pair.
        "g_1 & !g_True",
        "r1 & !rTrue",
        "a0 & !aFalse",
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

// black parses "a W b" as weak until, as SPOT writes it, but its
// infinite-trace encoding is unsound on the strong-release its NNF derives
// from a negated one: the loop-closure rules treat only F and U as
// eventualities, so nothing forces M's obligation and black reports SAT for a
// negation that is UNSAT. Invoked directly on any case below it gets them
// wrong. check_satisfiability rewrites W and M away first, so these are the
// cases that pin the rewrite -- each is a validity check, the shape that puts
// a W under exactly one negation.
void test_weak_until_validity(const std::chrono::milliseconds& timeout) {
    struct Case {
        const char* formula;
        bool satisfiable;
    };
    const std::array<Case, 5> cases{{
        // The three below are guarantees the 2026-08-07 elitism campaign
        // actually wrote out as repairs: each is valid, so each says nothing,
        // and the vacuity screen should have caught all three. ltlfilt's
        // --simplify does not fold any of them to a constant, so they reach
        // black and pin the rewrite rather than the constant-folding path in
        // front of it.
        {"!(F((r_1) W (X(!(r_1)))))", false},
        {"!(G(F((g1) W (X(!(g1))))))", false},
        {"!(F(((g_1) W (!(F(r_1)))) | (!(X(X(g_1))))))", false},
        // A W that genuinely is falsifiable must stay falsifiable: the rewrite
        // has to preserve the answer, not force every weak-until query to
        // UNSAT.
        {"!((a) W (b))", true},
        // G a entails a W b, the weak operator's defining case. ltlfilt folds
        // this one to a constant before black is consulted, so it pins the
        // first line of defence rather than the rewrite -- kept for the same
        // reason test_boolean_constants keeps its folded cases.
        {"!((G(a)) -> ((a) W (b)))", false},
    }};
    SatisfiabilityChecker checker;
    checker.set_timeout(timeout);
    for (const Case& test_case : cases) {
        const std::optional<bool> result =
            checker.check_satisfiability(test_case.formula);
        if (!result.has_value()) {
            fail(std::string("black-runner: ") + test_case.formula +
                 " should be decided, not indeterminate");
            continue;
        }
        expect(*result == test_case.satisfiable,
               std::string("black-runner: ") + test_case.formula +
                   " should be " +
                   (test_case.satisfiable ? "satisfiable" : "unsatisfiable"));
    }
}

// The lexical guard must not fire on atoms that merely contain W or M, or
// every formula over such atoms would pay an ltlfilt exec it does not need.
void test_weak_operator_scan_respects_token_boundaries() {
    expect(!has_weak_operator("(G(WAIT)) & (F(ALARM))"),
           "ltlfilt: W and M inside atom names are not operators");
    expect(!has_weak_operator("(G(m_write)) | (F(w_M))"),
           "ltlfilt: W and M inside underscored atoms are not operators");
    expect(has_weak_operator("(a) W (b)"),
           "ltlfilt: a standalone W is an operator");
    expect(has_weak_operator("(a) M (b)"),
           "ltlfilt: a standalone M is an operator");
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
    test_weak_until_validity(timeout);
    test_weak_operator_scan_respects_token_boundaries();
}
