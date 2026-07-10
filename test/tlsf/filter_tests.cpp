#include <algorithm>
#include <string>
#include <vector>

#include "runner/black.hpp"
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

}  // namespace

void run_tlsf_filter_tests() {
    test_spec_implies_reflexive();
    test_spec_implies_weakening_direction();
    test_weakening_filter_keeps_only_weakenings();
    test_bloat_cap_filter_drops_oversized();
    test_implication_filter_keeps_maximal();
}
