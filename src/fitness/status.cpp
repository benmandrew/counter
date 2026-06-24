#include "fitness/status.hpp"

#include <algorithm>
#include <cassert>
#include <string>

#include "requirement.hpp"

double specification_status(const Specification& specification,
                            SatisfiabilityChecker& sat,
                            RealizabilityChecker& real) {
    // Check each requirement independently: requirements fire at different
    // times (different conditions, Trigger vs Continual), so their conditions
    // and responses do not need to be simultaneously satisfiable across the
    // whole spec — only within each individual requirement.
    auto check_all = [&](auto&& pred) {
        return std::all_of(specification.m_assumptions.begin(),
                           specification.m_assumptions.end(), pred) &&
               std::all_of(specification.m_guarantees.begin(),
                           specification.m_guarantees.end(), pred);
    };
    if (!check_all([&](const Requirement& req) {
            return sat.check_satisfiability(req.m_condition.to_string())
                .value_or(false);
        })) {
        return 0.0;
    }
    if (!check_all([&](const Requirement& req) {
            return sat.check_satisfiability(req.m_response.to_string())
                .value_or(false);
        })) {
        return 0.1;
    }
    if (!check_all([&](const Requirement& req) {
            const std::string cond_and_resp =
                "(" + req.m_condition.to_string() + ") & (" +
                req.m_response.to_string() + ")";
            return sat.check_satisfiability(cond_and_resp).value_or(false);
        })) {
        return 0.2;
    }
    if (!specification.m_guarantees.empty() &&
        !real.check_realizability(specification)) {
        return 0.5;
    }
    return 1.0;
}
