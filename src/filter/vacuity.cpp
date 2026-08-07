#include "filter/vacuity.hpp"

#include <optional>
#include <string>

bool specification_is_trivially_vacuous(const Specification& specification) {
    return specification_has_false_condition(specification);
}

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

bool specification_has_valid_guarantee(const Specification& specification,
                                       SatisfiabilityChecker& checker) {
    for (const Requirement& req : specification.m_guarantees) {
        // Keyed on the negated requirement alone, so the cache hits across
        // candidates and generations: offspring share most requirements with
        // their parents, and a locked guarantee is free after its first
        // evaluation.
        const std::optional<bool> falsifiable =
            checker.check_satisfiability("!(" + req.m_ltl + ")");
        // Timeout: treat as falsifiable, as the assumption check treats an
        // unknown answer as satisfiable. A non-answer never drops a candidate.
        if (!falsifiable.value_or(true)) {
            return true;
        }
    }
    return false;
}

bool specification_is_vacuous(const Specification& specification,
                              SatisfiabilityChecker& checker) {
    return specification_is_trivially_vacuous(specification) ||
           specification_has_valid_guarantee(specification, checker) ||
           specification_has_unsatisfiable_assumptions(specification, checker);
}

FilterFunction make_vacuity_filter(SatisfiabilityChecker& checker,
                                   std::size_t max_in_flight) {
    return make_predicate_filter(
        "vacuity",
        [&checker](const Specification& spec) {
            return !specification_is_vacuous(spec, checker);
        },
        max_in_flight);
}
