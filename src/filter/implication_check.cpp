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
