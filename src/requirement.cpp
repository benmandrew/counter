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
            } else {
                return "for " + std::to_string(value.m_ticks) + " ticks";
            }
        },
        timing);
}
