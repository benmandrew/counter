#include "fitness/status.hpp"

#include <stdexcept>
#include <string>

#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"

static SatisfiabilityChecker global_sat_checker;
static RealizabilityChecker global_real_checker;

double requirement_status(const Requirement& requirement) {
    if (!requirement.m_ltl.has_value()) {
        throw std::invalid_argument(
            "Requirement must have m_ltl set to compute status.");
    }
    const std::string a = requirement.m_trigger.to_string();
    const std::string g = requirement.m_response.to_string();
    if (!global_sat_checker.check_satisfiability(a)) {
        return 0.0;
    }
    if (!global_sat_checker.check_satisfiability(g)) {
        return 0.1;
    }
    if (!global_sat_checker.check_satisfiability("(" + a + ") & (" + g + ")")) {
        return 0.2;
    }
    if (!global_real_checker.check_realizability(requirement)) {
        return 0.5;
    }
    return 1.0;
}
