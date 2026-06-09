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

std::string requirement_to_ltl(const Requirement& requirement) {
    const std::string t = "(" + requirement.m_trigger.to_string() + ")";
    const std::string r = "(" + requirement.m_response.to_string() + ")";
    return std::visit(
        [&](const auto& v) -> std::string {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, timing::Immediately>) {
                return "G(" + t + " -> " + r + ")";
            } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                return "G(" + t + " -> X" + r + ")";
            } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                return "G(" + t + " -> F[0.." +
                       std::to_string(v.m_ticks) + "]" + r + ")";
            } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
                return "G(" + t + " -> G[0.." +
                       std::to_string(v.m_ticks) + "]" + r + ")";
            } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
                if (v.m_ticks == 0) {
                    return "G(" + t + " -> " + r + ")";
                }
                const std::string n = std::to_string(v.m_ticks);
                const std::string nm1 = std::to_string(v.m_ticks - 1);
                return "G(" + t + " -> (G[0.." + nm1 + "] !" + r +
                       " & F[" + n + ".." + n + "]" + r + "))";
            } else {
                return "G(" + t + " -> F" + r + ")";
            }
        },
        requirement.m_timing);
}
