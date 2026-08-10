#include "fitness/status.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <vector>

#include "filter/well_separation.hpp"
#include "requirement.hpp"

double status_score(const std::vector<std::string>& components,
                    SatisfiabilityChecker& sat,
                    const std::function<bool()>& is_realizable) {
    const bool all_satisfiable = std::all_of(
        components.begin(), components.end(),
        [&sat](const std::string& formula) {
            // An unanswered query counts as unsatisfiable, matching the rest
            // of the scoring path: a timeout must not promote a candidate
            // above one that was decided.
            return sat.check_satisfiability(formula).value_or(false);
        });
    if (!all_satisfiable) {
        return k_status_component_unsatisfiable;
    }
    return is_realizable() ? k_status_realizable : k_status_unrealizable;
}

double specification_status(const Specification& specification,
                            SatisfiabilityChecker& sat,
                            RealizabilityChecker& real) {
    // Requirements are checked one at a time rather than conjoined across the
    // specification: they fire at different times (different conditions,
    // Trigger vs Continual), so their conditions and responses need not be
    // simultaneously satisfiable. Testing `condition & response` per
    // requirement also subsumes testing either half alone, since an
    // unsatisfiable half makes the conjunction unsatisfiable.
    std::vector<std::string> components;
    components.reserve(specification.m_assumptions.size() +
                       specification.m_guarantees.size());
    const auto add = [&components](const std::vector<Requirement>& reqs) {
        for (const Requirement& req : reqs) {
            components.push_back("(" + req.m_condition.to_string() + ") & (" +
                                 req.m_response.to_string() + ")");
        }
    };
    add(specification.m_assumptions);
    add(specification.m_guarantees);

    return status_score(components, sat, [&specification, &real] {
        // No guarantees leaves the implication with a `true` consequent, which
        // is realizable whatever the assumptions say; skip the solver rather
        // than ask it a question with a known answer.
        const bool realizable =
            specification.m_guarantees.empty() ||
            real.check_realizability(specification).value_or(false);
        // Second, and only where the first said yes: a candidate that is
        // already unrealizable cannot be realizable for the wrong reason, and
        // the query is a whole ltlsynt call.
        return realizable &&
               !specification_is_not_well_separated(specification, real);
    });
}
