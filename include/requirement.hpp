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
    std::string trigger_name;
    std::string response_name;
    Timing timing;
    std::size_t tick_count = 0;
};

struct State {
    bool trigger_holds = false;
    bool response_holds = false;
    bool countdown_state = false;
    std::size_t countdown_ticks = 0;

    std::string label() const;
};

std::vector<State> canonical_states();

std::string to_string(Timing timing);
