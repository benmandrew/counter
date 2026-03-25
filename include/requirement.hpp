#pragma once

#include <cstddef>
#include <string>
#include <vector>

enum class Timing {
    Immediately,
    NextTimepoint,
    WithinTicks,
    ForTicks,
};

struct Requirement {
    std::string m_trigger_name;
    std::string m_response_name;
    Timing m_timing;
    std::size_t m_tick_count = 0;
};

struct State {
    bool m_trigger_holds = false;
    bool m_response_holds = false;
    bool m_countdown_state = false;
    std::size_t m_countdown_ticks = 0;

    std::string label() const;
};

std::vector<State> canonical_states();

std::string to_string(Timing timing);
