#include "filter/well_separation.hpp"

#include <string>

bool specification_is_not_well_separated(const Specification& specification,
                                         RealizabilityChecker& checker) {
    if (specification.m_assumptions.empty()) {
        return false;
    }
    std::string conjunction;
    for (const Requirement& req : specification.m_assumptions) {
        if (!conjunction.empty()) {
            conjunction += " & ";
        }
        conjunction += "(" + req.m_ltl + ")";
    }
    // Guarantees replaced with false: the spec becomes (assumptions) -> false,
    // i.e. !(assumptions). It is realizable exactly when the system has a
    // strategy that forces the assumptions to fail against every environment --
    // the definition of not being well-separated. The input/output partition is
    // the original spec's, so assumptions over input atoms alone stay
    // unrealizable (the system controls nothing it could use to break them).
    const std::string formula = "(" + conjunction + ") -> (false)";
    return checker.check_realizability_ltl(formula, specification.m_in_atoms,
                                           specification.m_out_atoms);
}

FilterFunction make_well_separation_filter(RealizabilityChecker& checker) {
    return make_predicate_filter(
        "not-well-separated", [&checker](const Specification& spec) {
            return !specification_is_not_well_separated(spec, checker);
        });
}
