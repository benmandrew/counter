#include "requirement.hpp"

#include <algorithm>
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

// Expands G[0..n-1](!R) & F[n..n] R as !R & X(!R & X(... & X R))
// (n nestings: n occurrences of !R followed by R). Caller handles n=0.
std::string expand_after(const std::string& response, std::size_t ticks) {
    std::string not_response = "!";
    not_response += response;
    std::string inner = response;
    for (std::size_t i = 0; i < ticks; ++i) {
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

Requirement::Requirement(Formula trigger, Formula response,
                         const Timing& timing)
    : m_trigger(std::move(trigger)),
      m_response(std::move(response)),
      m_timing(timing) {}

Requirement::Requirement(Formula trigger, Formula response,
                         const Timing& timing, const std::string& ltl)
    : m_trigger(std::move(trigger)),
      m_response(std::move(response)),
      m_timing(timing),
      m_ltl(ltl) {}

bool operator<(const Requirement& lhs, const Requirement& rhs) {
    if (lhs.m_timing < rhs.m_timing || rhs.m_timing < lhs.m_timing) {
        return lhs.m_timing < rhs.m_timing;
    }
    if (lhs.m_trigger < rhs.m_trigger) {
        return true;
    }
    if (rhs.m_trigger < lhs.m_trigger) {
        return false;
    }
    if (lhs.m_response < rhs.m_response) {
        return true;
    }
    if (rhs.m_response < lhs.m_response) {
        return false;
    }
    return lhs.m_ltl < rhs.m_ltl;
}

bool operator==(const Requirement& lhs, const Requirement& rhs) {
    return !(lhs.m_timing < rhs.m_timing) && !(rhs.m_timing < lhs.m_timing) &&
           lhs.m_trigger == rhs.m_trigger && lhs.m_response == rhs.m_response &&
           lhs.m_ltl == rhs.m_ltl;
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
    rendered += m_trigger_holds ? "P" : "~P";
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
            } else {
                return "eventually";
            }
        },
        timing);
}

std::string Requirement::to_string() const {
    return "If " + m_trigger.to_string() + ", " + ::to_string(m_timing) + " " +
           m_response.to_string();
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

std::string requirement_to_ltl(const Requirement& requirement) {
    const std::string trigger_str =
        "(" + requirement.m_trigger.to_string() + ")";
    const std::string response_str =
        "(" + requirement.m_response.to_string() + ")";
    return std::visit(
        [&](const auto& variant) -> std::string {
            using T = std::decay_t<decltype(variant)>;
            if constexpr (std::is_same_v<T, timing::Immediately>) {
                return "G(" + trigger_str + " -> " + response_str + ")";
            } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                return "G(" + trigger_str + " -> X" + response_str + ")";
            } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                return "G(" + trigger_str + " -> (" +
                       expand_within(response_str, variant.m_ticks) + "))";
            } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                return "G(" + trigger_str + " -> (" +
                       expand_for(response_str, variant.m_ticks) + "))";
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                if (variant.m_ticks == 0) {
                    return "G(" + trigger_str + " -> " + response_str + ")";
                }
                return "G(" + trigger_str + " -> (" +
                       expand_after(response_str, variant.m_ticks) + "))";
            } else {
                return "G(" + trigger_str + " -> F" + response_str + ")";
            }
        },
        requirement.m_timing);
}
