#include "filter/implication_check.hpp"

#include <atomic>
#include <optional>
#include <string>

#include "filter/implication.hpp"
#include "requirement.hpp"

std::optional<bool> spec_implies(const Specification& from,
                                 const Specification& dest,
                                 SatisfiabilityChecker& checker) {
    if (from == dest) {
        return true;
    }
    const std::optional<bool> sat = checker.check_satisfiability(
        "(" + from.to_ltl() + ") & !(" + dest.to_ltl() + ")",
        QueryPolarity::ExpectUnsat);
    if (!sat.has_value()) {
        ImplicationFilterStats::n_timeouts.fetch_add(1,
                                                     std::memory_order_relaxed);
        return std::nullopt;
    }
    return !sat.value();
}
