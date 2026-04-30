#include "fitness/status.hpp"

#include <cassert>
#include <string>

#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"

static SatisfiabilityChecker global_sat_checker;
static RealizabilityChecker global_real_checker;

double specification_status(const Specification& specification) {
    for (const Requirement& req : specification.m_requirements) {
        assert(req.m_ltl.has_value());
    }
    // Build conjunctions of all triggers (assumptions) and all responses
    // (guarantees)
    std::string conj_a;
    std::string conj_g;
    bool first = true;
    for (const Requirement& req : specification.m_requirements) {
        if (!first) {
            conj_a += " & ";
            conj_g += " & ";
        }
        conj_a += "(" + req.m_trigger.to_string() + ")";
        conj_g += "(" + req.m_response.to_string() + ")";
        first = false;
    }
    std::string conj_ag = "(" + conj_a + ") & (" + conj_g + ")";
    if (!global_sat_checker.check_satisfiability(conj_a)) {
        return 0.0;
    }
    if (!global_sat_checker.check_satisfiability(conj_g)) {
        return 0.1;
    }
    if (!global_sat_checker.check_satisfiability(conj_ag)) {
        return 0.2;
    }
    // For realizability, check the whole specification
    if (!specification.m_requirements.empty() &&
        !global_real_checker.check_realizability(specification)) {
        return 0.5;
    }
    return 1.0;
}
