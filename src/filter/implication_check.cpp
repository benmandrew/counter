#include "filter/implication_check.hpp"

#include <algorithm>
#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "filter/implication.hpp"
#include "requirement.hpp"

namespace {

bool requirement_implies(const Requirement& from, const Requirement& dest,
                         SatisfiabilityChecker& checker) {
    if (from == dest) {
        return true;
    }
    // Propositional shortcut: when condition, timing, and condition_type are
    // identical the full LTL implication reduces to a propositional check on
    // the response formulas alone.  Propositional SAT is cheap and avoids the
    // black timeout that can block valid weakenings when many checks run
    // concurrently (the nested X-operator formulae from expand_for /
    // expand_within are complex enough to time out under thread-pool load).
    if (from.m_condition == dest.m_condition &&
        from.m_timing == dest.m_timing &&
        from.m_condition_type == dest.m_condition_type) {
        const std::string prop_check = "(" + from.m_response.to_string() +
                                       ") & !(" + dest.m_response.to_string() +
                                       ")";
        const auto prop_sat = checker.check_satisfiability(prop_check);
        if (prop_sat.has_value() && !prop_sat.value()) {
            return true;
        }
    }
    assert(from.m_ltl.has_value() && dest.m_ltl.has_value());
    const std::optional<bool> sat = checker.check_satisfiability(
        "(" + *from.m_ltl + ") & !(" + *dest.m_ltl + ")");
    if (!sat.has_value()) {
        ImplicationFilterStats::n_timeouts.fetch_add(1,
                                                     std::memory_order_relaxed);
        return false;
    }
    return !sat.value();
}

bool all_implied_by_some(const std::vector<Requirement>& from_reqs,
                         const std::vector<Requirement>& to_reqs,
                         SatisfiabilityChecker& checker) {
    return std::all_of(
        to_reqs.begin(), to_reqs.end(), [&](const Requirement& dest) {
            return std::any_of(from_reqs.begin(), from_reqs.end(),
                               [&](const Requirement& from) {
                                   return requirement_implies(from, dest,
                                                              checker);
                               });
        });
}

}  // namespace

bool spec_implies(const Specification& from, const Specification& dest,
                  SatisfiabilityChecker& checker) {
    return all_implied_by_some(dest.m_assumptions, from.m_assumptions,
                               checker) &&
           all_implied_by_some(from.m_guarantees, dest.m_guarantees, checker);
}
