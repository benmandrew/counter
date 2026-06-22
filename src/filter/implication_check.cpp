#include "filter/implication_check.hpp"

#include <cassert>
#include <optional>
#include <string>
#include <vector>

#include "filter/implication.hpp"
#include "requirement.hpp"

namespace {

// Returns nullopt if the LTL satisfiability check times out (caller decides
// how to handle uncertainty).
std::optional<bool> requirement_implies(const Requirement& from,
                                        const Requirement& dest,
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
        return std::nullopt;
    }
    return !sat.value();
}

// Returns true if every requirement in to_reqs is definitely implied by some
// requirement in from_reqs, false if any to_req is definitely not implied, or
// nullopt if some to_req has no definite implication but at least one timed-out
// check that could have confirmed it.
std::optional<bool> all_implied_by_some(
    const std::vector<Requirement>& from_reqs,
    const std::vector<Requirement>& to_reqs, SatisfiabilityChecker& checker) {
    bool any_uncertain = false;
    for (const Requirement& dest : to_reqs) {
        bool dest_implied = false;
        bool dest_uncertain = false;
        for (const Requirement& from : from_reqs) {
            const auto result = requirement_implies(from, dest, checker);
            if (result.value_or(false)) {
                dest_implied = true;
                break;
            }
            if (!result.has_value()) {
                dest_uncertain = true;
            }
        }
        if (!dest_implied) {
            if (dest_uncertain) {
                any_uncertain = true;
            } else {
                return false;
            }
        }
    }
    return any_uncertain ? std::optional<bool>{std::nullopt} : true;
}

}  // namespace

std::optional<bool> spec_implies(const Specification& from,
                                 const Specification& dest,
                                 SatisfiabilityChecker& checker) {
    const auto assumptions_ok =
        all_implied_by_some(dest.m_assumptions, from.m_assumptions, checker);
    if (assumptions_ok.has_value() && !assumptions_ok.value()) {
        return false;
    }
    const auto guarantees_ok =
        all_implied_by_some(from.m_guarantees, dest.m_guarantees, checker);
    if (guarantees_ok.has_value() && !guarantees_ok.value()) {
        return false;
    }
    if (!assumptions_ok.has_value() || !guarantees_ok.has_value()) {
        return std::nullopt;
    }
    return true;
}
