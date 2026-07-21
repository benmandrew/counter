#include "filter/vacuity.hpp"

#include <optional>
#include <string>

bool specification_has_unsatisfiable_assumptions(
    const Specification& specification, SatisfiabilityChecker& checker) {
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
    const std::optional<bool> sat = checker.check_satisfiability(conjunction);
    // Timeout: treat as satisfiable. Dropping on an unknown answer would make
    // the filter's verdict depend on machine load.
    return sat.has_value() && !sat.value();
}

FilterFunction make_vacuity_filter(SatisfiabilityChecker& checker) {
    return make_predicate_filter(
        "vacuous-assumptions", [&checker](const Specification& spec) {
            return !specification_has_unsatisfiable_assumptions(spec, checker);
        });
}
