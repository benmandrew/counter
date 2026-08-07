#include <algorithm>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

tlsf::Specification parse_spec(const std::string& main_body) {
    return tlsf::parse("INFO { SEMANTICS: Mealy; }\nMAIN {\n" + main_body +
                       "\n}\n");
}

// A base guarantee-only spec and a strictly weaker variant that adds a fairness
// assumption. Adding an assumption is a logical weakening: base => weaker but
// not conversely.
tlsf::Specification base_spec() {
    return parse_spec("INPUTS { a; } OUTPUTS { b; } GUARANTEE { G (a -> b); }");
}

tlsf::Specification weaker_spec() {
    return parse_spec(
        "INPUTS { a; } OUTPUTS { b; } ASSUME { G F a; } "
        "GUARANTEE { G (a -> b); }");
}

// --- tlsf_spec_implies ---

void test_spec_implies_reflexive() {
    SatisfiabilityChecker& checker = global_sat_checker();
    const tlsf::Specification spec = base_spec();
    expect(tlsf_spec_implies(spec, spec, checker).value_or(false),
           "spec_implies: a specification implies itself");
}

void test_spec_implies_weakening_direction() {
    SatisfiabilityChecker& checker = global_sat_checker();
    const tlsf::Specification base = base_spec();
    const tlsf::Specification weaker = weaker_spec();
    expect(tlsf_spec_implies(base, weaker, checker).value_or(false),
           "spec_implies: the original implies the assumption-added weakening");
    expect(!tlsf_spec_implies(weaker, base, checker).value_or(true),
           "spec_implies: the weakening does not imply the original");
}

// --- assumption satisfiability ---

// The predicate behind both the per-generation vacuity filter and the
// final repair screen in tlsf::run_repair, so a vacuously-realizable elite
// cannot be written out as a repair.
void test_unsatisfiable_assumptions_detected() {
    SatisfiabilityChecker& checker = global_sat_checker();

    // Contradictory assumptions: the antecedent of (A) -> (G) is false, so the
    // spec is realizable for free without repairing anything.
    const tlsf::Specification contradictory = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } ASSUME { G a; G !a; } "
        "GUARANTEE { G (a -> b); }");
    expect(tlsf_has_unsatisfiable_assumptions(contradictory, checker),
           "vacuity: contradictory assumptions are detected as vacuous");

    expect(!tlsf_has_unsatisfiable_assumptions(weaker_spec(), checker),
           "vacuity: a satisfiable fairness assumption is not vacuous");

    // Conservative on the empty case: no assumptions is not vacuous, and the
    // check must not reach the solver at all.
    expect(!tlsf_has_unsatisfiable_assumptions(base_spec(), checker),
           "vacuity: a spec with no assumptions is kept");
}

void test_vacuity_filter_drops_contradictory_assumptions() {
    const tlsf::Specification contradictory = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } ASSUME { G a; G !a; } "
        "GUARANTEE { G (a -> b); }");
    const tlsf::Specification sound = weaker_spec();
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_vacuity_filter();
    const std::vector<tlsf::Specification> survivors =
        filter({contradictory, sound, base_spec()});
    expect(survivors.size() == 2,
           "vacuity filter: the vacuous candidate is dropped, the satisfiable "
           "and the assumption-free ones kept");
    expect(std::none_of(survivors.begin(), survivors.end(),
                        [&contradictory](const tlsf::Specification& kept) {
                            return kept == contradictory;
                        }),
           "vacuity filter: the dropped candidate is the contradictory one");
}

// --- the syntactic half ---

// `true` and `false` are ordinary atoms in this AST, so the section formulae
// below parse as atoms and the screen reads them without a solver. Both specs
// have satisfiable assumption sides, so the solver would keep them: the screen
// is what rejects them, and it runs first.
void test_trivial_section_literals_detected() {
    SatisfiabilityChecker& checker = global_sat_checker();

    const tlsf::Specification false_assumption = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } ASSUME { G F a; false; } "
        "GUARANTEE { G (a -> b); }");
    expect(tlsf_is_trivially_vacuous(false_assumption),
           "vacuity: a false ASSUME formula is trivially vacuous");
    expect(tlsf_is_vacuous(false_assumption, checker),
           "vacuity: the syntactic screen rejects what the solver would keep");

    const tlsf::Specification true_guarantee = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } GUARANTEE { G (a -> b); true; }");
    expect(tlsf_is_trivially_vacuous(true_guarantee),
           "vacuity: a true GUARANTEE formula is trivially vacuous");

    const tlsf::Specification true_assert =
        parse_spec("INPUTS { a; } OUTPUTS { b; } ASSERT { true; }");
    expect(tlsf_is_trivially_vacuous(true_assert),
           "vacuity: a true ASSERT formula is trivially vacuous");

    // The two sides are not symmetric: a `true` assumption is a no-op the
    // filter has no reason to reject, and a `false` guarantee makes the spec
    // unrealizable, which the search punishes on its own.
    const tlsf::Specification true_assumption = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } ASSUME { true; } "
        "GUARANTEE { G (a -> b); }");
    expect(!tlsf_is_trivially_vacuous(true_assumption),
           "vacuity: a true ASSUME formula is not trivially vacuous");

    const tlsf::Specification false_guarantee =
        parse_spec("INPUTS { a; } OUTPUTS { b; } GUARANTEE { false; }");
    expect(!tlsf_is_trivially_vacuous(false_guarantee),
           "vacuity: a false GUARANTEE formula is not trivially vacuous");

    expect(!tlsf_is_trivially_vacuous(weaker_spec()),
           "vacuity: an ordinary spec carries no trivial section literal");
}

void test_vacuity_filter_drops_trivial_section_literals() {
    const tlsf::Specification true_guarantee = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } GUARANTEE { G (a -> b); true; }");
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_vacuity_filter();
    const std::vector<tlsf::Specification> survivors =
        filter({true_guarantee, base_spec()});
    expect(survivors.size() == 1 && survivors[0] == base_spec(),
           "vacuity filter: the true-guarantee candidate is dropped");
}

// --- the semantic guarantee half ---

// `G (a | !a)` is valid but its root is a G, so the atom-only syntactic screen
// cannot see it. The parser does not simplify, so this also pins that the
// verdict does not depend on a simplification pass having run.
void test_valid_guarantee_caught_only_semantically() {
    SatisfiabilityChecker& checker = global_sat_checker();
    const tlsf::Specification tautological =
        parse_spec("INPUTS { a; } OUTPUTS { b; } GUARANTEE { G (a | !a); }");
    expect(!tlsf_is_trivially_vacuous(tautological),
           "vacuity: G (a | !a) is not a trivial section literal");
    expect(tlsf_has_valid_guarantee(tautological, checker),
           "vacuity: G (a | !a) is a valid guarantee");
    expect(tlsf_is_vacuous(tautological, checker),
           "vacuity: a valid guarantee makes the spec vacuous");

    expect(!tlsf_has_valid_guarantee(base_spec(), checker),
           "vacuity: G (a -> b) is falsifiable and demands something");
}

// Per formula, not over the guarantee conjunction: one gutted conjunct is
// enough, wherever it sits, so the early exit cannot change the verdict.
void test_one_valid_guarantee_among_substantive_ones_rejects() {
    SatisfiabilityChecker& checker = global_sat_checker();
    expect(tlsf_has_valid_guarantee(
               parse_spec("INPUTS { a; } OUTPUTS { b; } "
                          "GUARANTEE { G (a | !a); G (a -> b); }"),
               checker),
           "vacuity: a valid guarantee is caught when it comes first");
    expect(tlsf_has_valid_guarantee(
               parse_spec("INPUTS { a; } OUTPUTS { b; } "
                          "GUARANTEE { G (a -> b); G (a | !a); }"),
               checker),
           "vacuity: a valid guarantee is caught when it comes last");
    // ASSERT is G-wrapped by the lowering, and `G psi` is valid exactly when
    // psi is, so the raw formula is the query.
    expect(tlsf_has_valid_guarantee(
               parse_spec("INPUTS { a; } OUTPUTS { b; } "
                          "ASSERT { a | !a; } GUARANTEE { G (a -> b); }"),
               checker),
           "vacuity: a valid ASSERT formula is caught unwrapped");
}

// A non-answer keeps the candidate, matching the assumption side. 1ms cannot
// spawn a subprocess, and `!(G (a -> b))` does not fold to a constant, so the
// query reaches the deadline rather than being decided ahead of it.
void test_guarantee_timeout_keeps_the_candidate() {
    SatisfiabilityChecker checker;
    checker.set_timeout(std::chrono::milliseconds(1));
    const std::size_t before = SatisfiabilityChecker::n_timeouts;
    expect(!tlsf_has_valid_guarantee(base_spec(), checker),
           "vacuity: a timed-out guarantee query keeps the candidate");
    expect(SatisfiabilityChecker::n_timeouts == before + 1,
           "vacuity: the guarantee query did time out, so the keep was the "
           "timeout policy and not an answer");
}

// The assumption side takes no such split: unsatisfiability does not
// distribute over conjunction, so each assumption is satisfiable alone while
// their conjunction is not.
void test_assumption_side_stays_a_joint_query() {
    SatisfiabilityChecker& checker = global_sat_checker();
    expect(checker.check_satisfiability("G a").value_or(false) &&
               checker.check_satisfiability("G !a").value_or(false),
           "vacuity: 'G a' and 'G !a' are each satisfiable alone");
    expect(tlsf_has_unsatisfiable_assumptions(
               parse_spec("INPUTS { a; } OUTPUTS { b; } ASSUME { G a; G !a; } "
                          "GUARANTEE { G (a -> b); }"),
               checker),
           "vacuity: 'G a' and 'G !a' are jointly unsatisfiable");
}

// --- weakening filter ---

void test_weakening_filter_keeps_only_weakenings() {
    SatisfiabilityChecker& checker = global_sat_checker();
    const tlsf::Specification base = base_spec();
    const tlsf::Specification weaker = weaker_spec();
    // A strengthening: an extra guarantee the original does not impose.
    const tlsf::Specification stronger = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } GUARANTEE { G (a -> b); G b; }");
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_weakening_filter(base, checker);
    const std::vector<tlsf::Specification> survivors =
        filter({base, weaker, stronger});
    const auto has = [&survivors](const tlsf::Specification& spec) {
        return std::any_of(
            survivors.begin(), survivors.end(),
            [&spec](const tlsf::Specification& kept) { return kept == spec; });
    };
    expect(has(base), "weakening: the original itself is kept");
    expect(has(weaker), "weakening: a weakening of the original is kept");
    expect(!has(stronger), "weakening: a strengthening is dropped");
}

// --- bloat cap filter ---

void test_bloat_cap_filter_drops_oversized() {
    const tlsf::Specification original =
        parse_spec("INPUTS { a; } OUTPUTS { b; } GUARANTEE { G (a -> b); }");
    // A candidate with a much larger guarantee formula than the original's.
    const tlsf::Specification bloated = parse_spec(
        "INPUTS { a; } OUTPUTS { b; } "
        "GUARANTEE { G (((a & b) | (a & b)) -> ((a | b) & (a | b))); }");
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_bloat_cap_filter(original, 2.0);
    const std::vector<tlsf::Specification> survivors =
        filter({original, bloated});
    expect(survivors.size() == 1 && survivors.front() == original,
           "bloat: the oversized candidate is dropped, the original kept");
}

// --- implication (maximality) filter ---

void test_implication_filter_keeps_maximal() {
    SatisfiabilityChecker& checker = global_sat_checker();
    const tlsf::Specification base = base_spec();
    const tlsf::Specification weaker = weaker_spec();
    // base strictly dominates weaker (base => weaker, not conversely), so only
    // base is maximal.
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_implication_filter(checker);
    const std::vector<tlsf::Specification> maximal = filter({base, weaker});
    expect(maximal.size() == 1 && maximal.front() == base,
           "implication: only the dominating (stronger) spec is kept");
}

// `ASSUME { G F g }` over the output g: the system controls g and can simply
// never assert it, forcing the assumption to fail. `(G F g) -> false` is
// realizable, so the spec is vacuously satisfiable and not well-separated.
tlsf::Specification output_liveness_assumption_spec() {
    return parse_spec(
        "INPUTS { r; } OUTPUTS { g; } ASSUME { G F g; } "
        "GUARANTEE { G (r -> g); }");
}

// `ASSUME { G (g -> F r) }` mentions the output g, yet the system cannot force
// it to fail: falsifying needs `F(g & G !r)`, and r is an input the environment
// can hold false forever. A reactive-environment assumption that is still
// well-separated.
tlsf::Specification reactive_output_assumption_spec() {
    return parse_spec(
        "INPUTS { r; } OUTPUTS { g; } ASSUME { G (g -> F r); } "
        "GUARANTEE { G (r -> g); }");
}

void test_well_separation_drops_output_liveness_assumption() {
    RealizabilityChecker checker;
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_well_separation_filter(checker);
    const std::vector<tlsf::Specification> kept =
        filter({output_liveness_assumption_spec()});
    expect(kept.empty(),
           "well-separation: a spec whose assumption the system can force to "
           "fail (G F <output>) is dropped");
}

void test_well_separation_keeps_reactive_output_assumption() {
    RealizabilityChecker checker;
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_well_separation_filter(checker);
    const tlsf::Specification spec = reactive_output_assumption_spec();
    const std::vector<tlsf::Specification> kept = filter({spec});
    expect(kept.size() == 1 && kept.front() == spec,
           "well-separation: an output-referencing assumption the system "
           "cannot force to fail (G(<output> -> F <input>)) is kept");
}

void test_well_separation_keeps_input_only_assumption() {
    RealizabilityChecker checker;
    const FilterFunctionT<tlsf::Specification> filter =
        tlsf_make_well_separation_filter(checker);
    // Input-only assumption: well-separated by construction, kept without a
    // realizability query.
    const tlsf::Specification spec = weaker_spec();
    const std::vector<tlsf::Specification> kept = filter({spec});
    expect(kept.size() == 1 && kept.front() == spec,
           "well-separation: an input-only assumption is kept");
}

}  // namespace

void run_tlsf_filter_tests() {
    test_spec_implies_reflexive();
    test_spec_implies_weakening_direction();
    test_unsatisfiable_assumptions_detected();
    test_vacuity_filter_drops_contradictory_assumptions();
    test_trivial_section_literals_detected();
    test_vacuity_filter_drops_trivial_section_literals();
    test_valid_guarantee_caught_only_semantically();
    test_one_valid_guarantee_among_substantive_ones_rejects();
    test_guarantee_timeout_keeps_the_candidate();
    test_assumption_side_stays_a_joint_query();
    test_weakening_filter_keeps_only_weakenings();
    test_bloat_cap_filter_drops_oversized();
    test_implication_filter_keeps_maximal();
    test_well_separation_drops_output_liveness_assumption();
    test_well_separation_keeps_reactive_output_assumption();
    test_well_separation_keeps_input_only_assumption();
}
