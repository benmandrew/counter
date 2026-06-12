#include "fitness/status.hpp"

#include <cassert>
#include <string>

#include "requirement.hpp"

double specification_status(const Specification& specification,
                            SatisfiabilityChecker& sat,
                            RealizabilityChecker& real) {
    for ([[maybe_unused]] const Requirement& req :
         specification.m_assumptions) {
        assert(req.m_ltl.has_value());
    }
    for ([[maybe_unused]] const Requirement& req : specification.m_guarantees) {
        assert(req.m_ltl.has_value());
    }
    std::string conj_a;
    std::string conj_g;
    bool first = true;
    auto add_req = [&](const Requirement& req) {
        if (!first) {
            conj_a += " & ";
            conj_g += " & ";
        }
        conj_a += "(" + req.m_trigger.to_string() + ")";
        conj_g += "(" + req.m_response.to_string() + ")";
        first = false;
    };
    for (const Requirement& req : specification.m_assumptions) {
        add_req(req);
    }
    for (const Requirement& req : specification.m_guarantees) {
        add_req(req);
    }
    const std::string conj_ag = "(" + conj_a + ") & (" + conj_g + ")";
    if (!sat.check_satisfiability(conj_a)) {
        return 0.0;
    }
    if (!sat.check_satisfiability(conj_g)) {
        return 0.1;
    }
    if (!sat.check_satisfiability(conj_ag)) {
        return 0.2;
    }
    if (!specification.m_guarantees.empty() &&
        !real.check_realizability(specification)) {
        return 0.5;
    }
    return 1.0;
}
