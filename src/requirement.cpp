#include "requirement.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <set>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

bool atom_contains_uppercase(const std::string& atom) {
    return std::any_of(atom.begin(), atom.end(),
                       [](unsigned char chr) { return std::isupper(chr); });
}

// Expands F[0..n] R as R | X(R | X(... | X R)) (n nestings of X).
std::string expand_within(const std::string& response, std::size_t ticks) {
    std::string inner = response;
    for (std::size_t i = 0; i < ticks; ++i) {
        std::string next = response;
        next += " | X(";
        next += inner;
        next += ")";
        inner = std::move(next);
    }
    return inner;
}

// Expands G[0..n] R as R & X(R & X(... & X R)) (n nestings of X).
std::string expand_for(const std::string& response, std::size_t ticks) {
    std::string inner = response;
    for (std::size_t i = 0; i < ticks; ++i) {
        std::string next = response;
        next += " & X(";
        next += inner;
        next += ")";
        inner = std::move(next);
    }
    return inner;
}

// Expands G[0..n](!R) & F[n+1..n+1] R as !R & X(!R & X(... & X R))
// (n+1 nestings of !R followed by R). Implements FRET "after n":
// (for n ¬R) ∧ (within (n+1) R) = ¬R at t=0..n, R at t=n+1. For n=0 this is
// !R & X(R): R must not hold at the condition tick, but must hold at the
// next one.
std::string expand_after(const std::string& response, std::size_t ticks) {
    std::string not_response = "!";
    not_response += response;
    std::string inner = response;
    for (std::size_t i = 0; i <= ticks; ++i) {
        std::string next = not_response;
        next += " & X(";
        next += inner;
        next += ")";
        inner = std::move(next);
    }
    return inner;
}

}  // namespace

bool operator<(const Timing& lhs, const Timing& rhs) {
    if (lhs.index() != rhs.index()) {
        return lhs.index() < rhs.index();
    }
    const auto get_ticks = [](const Timing& tim) -> std::size_t {
        return std::visit(
            [](const auto& val) -> std::size_t {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, timing::WithinTicks> ||
                              std::is_same_v<T, timing::ForTicks> ||
                              std::is_same_v<T, timing::AfterTicks>) {
                    return val.m_ticks;
                }
                return 0;
            },
            tim);
    };
    return get_ticks(lhs) < get_ticks(rhs);
}

bool operator==(const Timing& lhs, const Timing& rhs) {
    return !(lhs < rhs) && !(rhs < lhs);
}

Requirement::Requirement(Formula condition, Formula response,
                         const Timing& timing, ConditionType condition_type,
                         bool weakenable)
    : m_condition(std::move(condition)),
      m_response(std::move(response)),
      m_timing(timing),
      m_condition_type(condition_type),
      m_ltl(requirement_to_ltl(*this)),
      m_weakenable(weakenable) {}

bool operator<(const Requirement& lhs, const Requirement& rhs) {
    if (lhs.m_timing < rhs.m_timing || rhs.m_timing < lhs.m_timing) {
        return lhs.m_timing < rhs.m_timing;
    }
    if (lhs.m_condition < rhs.m_condition) {
        return true;
    }
    if (rhs.m_condition < lhs.m_condition) {
        return false;
    }
    if (lhs.m_response < rhs.m_response) {
        return true;
    }
    if (rhs.m_response < lhs.m_response) {
        return false;
    }
    if (lhs.m_condition_type != rhs.m_condition_type) {
        return lhs.m_condition_type < rhs.m_condition_type;
    }
    if (lhs.m_ltl != rhs.m_ltl) {
        return lhs.m_ltl < rhs.m_ltl;
    }
    return !lhs.m_weakenable && rhs.m_weakenable;
}

bool operator==(const Requirement& lhs, const Requirement& rhs) {
    return !(lhs.m_timing < rhs.m_timing) && !(rhs.m_timing < lhs.m_timing) &&
           lhs.m_condition == rhs.m_condition &&
           lhs.m_response == rhs.m_response &&
           lhs.m_condition_type == rhs.m_condition_type &&
           lhs.m_ltl == rhs.m_ltl && lhs.m_weakenable == rhs.m_weakenable;
}

Specification::Specification(std::vector<Requirement> assumptions,
                             std::vector<Requirement> guarantees,
                             std::vector<std::string> in_atoms,
                             std::vector<std::string> out_atoms)
    : m_in_atoms(std::move(in_atoms)), m_out_atoms(std::move(out_atoms)) {
    auto deduplicate =
        [](std::vector<Requirement> reqs) -> std::vector<Requirement> {
        std::set<Requirement> seen;
        std::vector<Requirement> unique;
        for (auto& req : reqs) {
            auto [seen_iter, inserted] = seen.insert(req);
            if (inserted) {
                unique.push_back(std::move(req));
            }
        }
        return unique;
    };
    m_assumptions = deduplicate(std::move(assumptions));
    m_guarantees = deduplicate(std::move(guarantees));
    for (const std::string& atom : m_in_atoms) {
        if (atom_contains_uppercase(atom)) {
            std::cerr << "Warning: input atom '" << atom
                      << "' contains uppercase letters\n";
        }
    }
    for (const std::string& atom : m_out_atoms) {
        if (atom_contains_uppercase(atom)) {
            std::cerr << "Warning: output atom '" << atom
                      << "' contains uppercase letters\n";
        }
    }
}

bool operator<(const Specification& lhs, const Specification& rhs) {
    if (lhs.m_assumptions != rhs.m_assumptions) {
        return lhs.m_assumptions < rhs.m_assumptions;
    }
    return lhs.m_guarantees < rhs.m_guarantees;
}

bool operator==(const Specification& lhs, const Specification& rhs) {
    return lhs.m_assumptions == rhs.m_assumptions &&
           lhs.m_guarantees == rhs.m_guarantees &&
           lhs.m_in_atoms == rhs.m_in_atoms &&
           lhs.m_out_atoms == rhs.m_out_atoms;
}

std::string State::label() const {
    if (m_countdown_state) {
        return "c=" + std::to_string(m_countdown_ticks);
    }
    std::string rendered;
    rendered += m_condition_holds ? "P" : "~P";
    rendered += m_response_holds ? "Q" : "~Q";
    return rendered;
}

std::vector<State> canonical_states() {
    return {
        {false, false},
        {false, true},
        {true, false},
        {true, true},
    };
}

std::string to_string(const Timing& timing) {
    return std::visit(
        [](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, timing::Immediately>) {
                return "immediately";
            } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                return "at the next timepoint";
            } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                return "within " + std::to_string(value.m_ticks) + " ticks";
            } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                return "for " + std::to_string(value.m_ticks) + " ticks";
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                return "after " + std::to_string(value.m_ticks) + " ticks";
            } else if constexpr (std::is_same_v<T, timing::Eventually>) {
                return "eventually";
            } else {
                return "always";
            }
        },
        timing);
}

std::string to_string(ConditionType condition_type) {
    switch (condition_type) {
        case ConditionType::Trigger:
            return "upon";
        case ConditionType::Continual:
            return "whenever";
    }
    assert(false);
    __builtin_unreachable();
}

std::string Requirement::condition_to_string() const {
    // requirement_to_ltl only drops the G(...) wrapper for a true condition
    // when m_condition_type is Trigger (a trigger on an always-true
    // condition reduces to a bare initial obligation); for Continual it
    // always emits G(condition -> body), so the FRETish must keep an
    // explicit condition clause here too — omitting it makes the formaliser
    // CLI treat the requirement as unscoped and drop the G, silently
    // changing "always" into "only at the initial timepoint".
    if (m_condition == Formula::true_formula &&
        m_condition_type == ConditionType::Trigger) {
        return "";
    }
    return ::to_string(m_condition_type) + " " + m_condition.to_string();
}

std::string Requirement::to_string() const {
    return condition_to_string() + " C shall " + ::to_string(m_timing) +
           " satisfy " + m_response.to_string();
}

std::string Specification::to_string() const {
    std::string result;
    for (const Requirement& req : m_assumptions) {
        if (!result.empty()) {
            result += "\n";
        }
        result += req.to_string();
    }
    for (const Requirement& req : m_guarantees) {
        if (!result.empty()) {
            result += "\n";
        }
        result += req.to_string();
    }
    return result;
}

bool specification_has_false_condition(const Specification& specification) {
    auto condition_is_false = [](const Requirement& req) {
        return req.m_condition.atom_name() == "false";
    };
    return std::any_of(specification.m_assumptions.begin(),
                       specification.m_assumptions.end(), condition_is_false) ||
           std::any_of(specification.m_guarantees.begin(),
                       specification.m_guarantees.end(), condition_is_false);
}

std::string requirement_to_ltl(const Requirement& requirement) {
    const std::string condition_str =
        "(" + requirement.m_condition.to_string() + ")";
    std::string response_str = "(" + requirement.m_response.to_string() + ")";

    // Compute the timing body — the same expansion used for both condition
    // types; only the outer wrapper differs.
    const std::string body = std::visit(
        [&](const auto& variant) -> std::string {
            using T = std::decay_t<decltype(variant)>;
            if constexpr (std::is_same_v<T, timing::Immediately>) {
                return response_str;
            } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                return "X" + response_str;
            } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                return "(" + expand_within(response_str, variant.m_ticks) + ")";
            } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                return "(" + expand_for(response_str, variant.m_ticks) + ")";
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                return "(" + expand_after(response_str, variant.m_ticks) + ")";
            } else if constexpr (std::is_same_v<T, timing::Eventually>) {
                return "F" + response_str;
            } else {
                return "G" + response_str;
            }
        },
        requirement.m_timing);

    if (requirement.m_condition_type == ConditionType::Continual) {
        return "G(" + condition_str + " -> " + body + ")";
    }
    // Trigger: fires on rising edge (!C & X(C)) -> X(body), plus bare (C ->
    // body) at t=0.  When C is the constant true the rising-edge clause is
    // vacuously true (!(true) is always false), so the formula collapses to
    // just the initial obligation.  Emitting the full G form causes black's
    // BMC to time out on deeply-nested X bodies even though it is trivially
    // satisfied.
    if (requirement.m_condition.to_string() == "true") {
        return condition_str + " -> " + body;
    }
    return "G((!" + condition_str + " & X" + condition_str + ") -> X(" + body +
           ")) & (" + condition_str + " -> " + body + ")";
}
