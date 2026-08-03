#include <chrono>
#include <cstddef>
#include <string>
#include <thread>

#include "config.hpp"
#include "runner/ltlfilt.hpp"
#include "runner/spot_inprocess.hpp"
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

// simplify_ltl surfaces SPOT's boolean constants so callers can decide the
// formula without a solver; normalize_ltl hides them behind the original
// formula so its result stays safe to hand to a downstream tool.
void test_constants_surface_only_in_simplify() {
    expect(simplify_ltl("p & !p") == "0",
           "ltlfilt-runner: a contradiction should simplify to \"0\"");
    expect(simplify_ltl("p | !p") == "1",
           "ltlfilt-runner: a tautology should simplify to \"1\"");
    expect(normalize_ltl("p & !p") == "p & !p",
           "ltlfilt-runner: normalize_ltl should fall back to the original "
           "formula when it reduces to a constant");
    expect(normalize_ltl("p | !p") == "p | !p",
           "ltlfilt-runner: normalize_ltl should fall back to the original "
           "formula when it reduces to a constant");
}

// The "true"/"false" atoms this codebase uses for its boolean constants are
// real constants to SPOT, which is what lets them be folded away.
void test_boolean_constant_atoms_fold() {
    expect(simplify_ltl("G(false)") == "0",
           "ltlfilt-runner: G(false) should simplify to \"0\"");
    expect(simplify_ltl("G(true)") == "1",
           "ltlfilt-runner: G(true) should simplify to \"1\"");
}

// Simplifying either of these takes about 0.25 seconds, measured; see the note
// on the matching constant in spot_inprocess_tests.cpp for the shape and why
// this size. Two of them, over disjoint atoms, so the two engines tested below
// cannot share a cache entry and each really runs.
const char* const k_slow_for_libspot =
    "G(b0L -> ((a0L) | X(a0L) | XX(a0L) | XXX(a0L) | XXXX(a0L) | XXXXX(a0L) | "
    "XXXXXX(a0L))) & G(b1L -> ((a1L) | X(a1L) | XX(a1L) | XXX(a1L) | "
    "XXXX(a1L) | XXXXX(a1L) | XXXXXX(a1L))) & G(b2L -> ((a2L) | X(a2L) | "
    "XX(a2L) | XXX(a2L) | XXXX(a2L) | XXXXX(a2L) | XXXXXX(a2L)))";

const char* const k_slow_for_ltlfilt =
    "G(b0F -> ((a0F) | X(a0F) | XX(a0F) | XXX(a0F) | XXXX(a0F) | XXXXX(a0F) | "
    "XXXXXX(a0F))) & G(b1F -> ((a1F) | X(a1F) | XX(a1F) | XXX(a1F) | "
    "XXXX(a1F) | XXXXX(a1F) | XXXXXX(a1F))) & G(b2F -> ((a2F) | X(a2F) | "
    "XX(a2F) | XXX(a2F) | XXXX(a2F) | XXXXX(a2F) | XXXXXX(a2F)))";

// The timeout's contract at this layer, and the reason it is safe to have one
// at all: a formula that outruns it comes back unsimplified rather than
// dropped, thrown, or half-simplified. That is already what this function
// returns when there is no ltlfilt to run, so nothing downstream meets a case
// it has not always had to handle.
//
// Run against both engines, because the timeout applies to the operation rather
// than to where it runs, and the two enforce it by entirely different means --
// abandoning a thread in process, killing a child out of it.
void test_a_timed_out_simplification_returns_the_formula_unchanged() {
    for (const SimplifyEngine engine :
         {SimplifyEngine::Libspot, SimplifyEngine::Ltlfilt}) {
        const std::string formula = engine == SimplifyEngine::Libspot
                                        ? k_slow_for_libspot
                                        : k_slow_for_ltlfilt;
        set_simplify_engine(engine);
        set_simplify_timeout(std::chrono::milliseconds(10));
        const std::string result = simplify_ltl(formula);
        set_simplify_timeout(std::chrono::milliseconds(0));
        set_simplify_engine(SimplifyEngine::Libspot);
        expect(result == formula,
               "ltlfilt-runner: a simplification past its timeout should "
               "return the formula unchanged");
        // The in-process engine leaves a worker holding libspot, and until it
        // finishes every later call in this process falls back to the tool.
        // Answers stay equivalent either way, but they are not byte-identical
        // -- the two orderings differ -- so leaving one running would make the
        // tests that follow depend on the timing of this one.
        const auto give_up =
            std::chrono::steady_clock::now() + std::chrono::seconds(120);
        while (spot_abandoned_workers() > 0 &&
               std::chrono::steady_clock::now() < give_up) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        expect(spot_abandoned_workers() == 0,
               "ltlfilt-runner: the abandoned simplification must finish "
               "before the next test runs");
    }
}

}  // namespace

void run_ltlfilt_runner_tests() {
    test_idempotent();
    test_reorders_atomic_propositions();
    test_invalid_formula_returns_original();
    test_valid_ltl_formula_normalises();
    test_constants_surface_only_in_simplify();
    test_boolean_constant_atoms_fold();
    test_a_timed_out_simplification_returns_the_formula_unchanged();
}
