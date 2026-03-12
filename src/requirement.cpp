#include "requirement.hpp"

std::string State::label() const {
    if (countdown_state) {
        return "c=" + std::to_string(countdown_ticks);
    }

    std::string rendered;
    rendered += trigger_holds ? "P" : "~P";
    rendered += response_holds ? "Q" : "~Q";
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

std::string to_string(Timing timing) {
    switch (timing) {
        case Timing::Immediately:
            return "immediately";
        case Timing::NextTimepoint:
            return "at the next timepoint";
        case Timing::WithinTicks:
            return "within N ticks";
        case Timing::ForTicks:
            return "for N ticks";
    }

    return "unknown";
}
