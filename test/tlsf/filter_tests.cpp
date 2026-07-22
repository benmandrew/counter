#include <algorithm>
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
    test_weakening_filter_keeps_only_weakenings();
    test_bloat_cap_filter_drops_oversized();
    test_implication_filter_keeps_maximal();
    test_well_separation_drops_output_liveness_assumption();
    test_well_separation_keeps_reactive_output_assumption();
    test_well_separation_keeps_input_only_assumption();
}
