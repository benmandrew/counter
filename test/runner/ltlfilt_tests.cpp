#include <string>

#include "runner/ltlfilt.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_idempotent() {
    const std::string formula = "G(p -> F(q))";
    const std::string once = normalize_ltl(formula);
    const std::string twice = normalize_ltl(once);
    expect(once == twice, "ltlfilt-runner: normalize_ltl should be idempotent");
}

void test_reorders_atomic_propositions() {
    // These formulae differ only in the order of their conjuncts — no
    // simplification could make them identical.  If ltlfilt canonicalises
    // the AP order both must produce the same string.
    const std::string norm_pq = normalize_ltl("p & q");
    const std::string norm_qp = normalize_ltl("q & p");
    expect(norm_pq == norm_qp,
           "ltlfilt-runner: p & q and q & p should normalise to the same form");
    const std::string norm_pqr = normalize_ltl("p & q & r");
    const std::string norm_rpq = normalize_ltl("r & p & q");
    const std::string norm_qrp = normalize_ltl("q & r & p");
    expect(norm_pqr == norm_rpq,
           "ltlfilt-runner: p&q&r and r&p&q should normalise to the same form");
    expect(norm_pqr == norm_qrp,
           "ltlfilt-runner: p&q&r and q&r&p should normalise to the same form");
}

void test_invalid_formula_returns_original() {
    // An unparseable formula must not throw; it returns the original string.
    const std::string bad = "G(";
    const std::string result = normalize_ltl(bad);
    expect(result == bad,
           "ltlfilt-runner: invalid formula should be returned unchanged");
}

void test_valid_ltl_formula_normalises() {
    // A well-formed LTL formula should survive normalisation and remain
    // non-empty.
    const std::string formula = "G(F(p))";
    const std::string result = normalize_ltl(formula);
    expect(!result.empty(),
           "ltlfilt-runner: normalised formula should be non-empty");
}

}  // namespace

void run_ltlfilt_runner_tests() {
    test_idempotent();
    test_reorders_atomic_propositions();
    test_invalid_formula_returns_original();
    test_valid_ltl_formula_normalises();
}
