#pragma once

#include <string>
#include <vector>

enum class Timing {
    Immediately,
    NextTimepoint,
};

struct Requirement {
    std::string trigger_name;
    std::string response_name;
    Timing timing;
};

struct State {
    bool trigger_holds;
    bool response_holds;

    std::string label() const;
};

std::vector<State> canonical_states();

std::string to_string(Timing timing);
