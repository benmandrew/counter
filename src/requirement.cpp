#include "requirement.hpp"

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

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
                return "G(" + trigger_str + " -> F[0.." +
                       std::to_string(variant.m_ticks) + "]" + response_str +
                       ")";
            } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                return "G(" + trigger_str + " -> G[0.." +
                       std::to_string(variant.m_ticks) + "]" + response_str +
                       ")";
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                if (variant.m_ticks == 0) {
                    return "G(" + trigger_str + " -> " + response_str + ")";
                }
                const std::string ticks_str = std::to_string(variant.m_ticks);
                const std::string ticks_minus1_str =
                    std::to_string(variant.m_ticks - 1);
                return "G(" + trigger_str + " -> (G[0.." + ticks_minus1_str +
                       "] !" + response_str + " & F[" + ticks_str + ".." +
                       ticks_str + "]" + response_str + "))";
            } else {
                return "G(" + trigger_str + " -> F" + response_str + ")";
            }
        },
        requirement.m_timing);
}
