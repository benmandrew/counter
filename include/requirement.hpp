#pragma once

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "prop_formula.hpp"

namespace timing {

/// Response must hold immediately when trigger is true.
struct Immediately {};

/// Response must hold at the next timepoint after trigger.
struct NextTimepoint {};

/// Response must hold within `m_ticks` ticks of trigger, including the trigger
/// tick.
struct WithinTicks {
    std::size_t m_ticks;
};

/// Response must hold for `m_ticks` consecutive ticks, including the trigger
/// tick.
struct ForTicks {
    std::size_t m_ticks;
};

/// Algebraic data type for requirement timing.
using Timing = std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks>;

inline Timing immediately() { return Immediately{}; }

inline Timing next_timepoint() { return NextTimepoint{}; }

inline Timing within_ticks(std::size_t ticks) { return WithinTicks{ticks}; }

inline Timing for_ticks(std::size_t ticks) { return ForTicks{ticks}; }

}  // namespace timing

using Timing = timing::Timing;

/// A FRET requirement specifying a system obligation. Consists of a trigger
/// condition and a response that must be satisfied according to the specified
/// timing constraint. These are used as the basic units for repair and for
/// computing semantic and syntactic similarity metrics in genetic algorithms.
struct Requirement {
    /// The trigger condition (propositional formula)
    Formula m_trigger;
    /// The response obligation (propositional formula)
    Formula m_response;
    /// The timing constraint for satisfaction
    Timing m_timing;
};

/// A state in the automaton used for transfer matrix model counting. Encodes
/// the state of a requirement automaton at a particular timepoint, used to
/// compute both individual and joint requirement model counts.
struct State {
    /// Whether the trigger condition holds
    bool m_trigger_holds = false;
    /// Whether the response condition holds
    bool m_response_holds = false;
    /// Countdown mechanism state (for timed constraints)
    bool m_countdown_state = false;
    /// Remaining ticks in countdown
    std::size_t m_countdown_ticks = 0;

    /// Returns a human-readable label for this state (used for debugging).
    std::string label() const;
};

/// Returns the set of canonical states based on the cross-product of trigger
/// and response boolean valuations: {(T,R) | T,R ∈ {true, false}}.
std::vector<State> canonical_states();

/// Converts a Timing enum value to a human-readable string representation.
std::string to_string(const Timing& timing);
