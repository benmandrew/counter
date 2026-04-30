#pragma once

#include <cstddef>
#include <iostream>
#include <optional>
#include <set>
#include <string>
#include <utility>
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

inline bool operator<(const Timing& lhs, const Timing& rhs) {
    if (lhs.index() != rhs.index()) {
        return lhs.index() < rhs.index();
    }
    if (const auto* l = std::get_if<timing::WithinTicks>(&lhs)) {
        return l->m_ticks < std::get<timing::WithinTicks>(rhs).m_ticks;
    }
    if (const auto* l = std::get_if<timing::ForTicks>(&lhs)) {
        return l->m_ticks < std::get<timing::ForTicks>(rhs).m_ticks;
    }
    return false;
}

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
    /// An LTL formula representing the requirement semantics
    std::optional<std::string> m_ltl;

    friend bool operator<(const Requirement& lhs, const Requirement& rhs) {
        if (lhs.m_trigger < rhs.m_trigger) return true;
        if (rhs.m_trigger < lhs.m_trigger) return false;
        if (lhs.m_response < rhs.m_response) return true;
        if (rhs.m_response < lhs.m_response) return false;
        if (lhs.m_timing < rhs.m_timing) return true;
        if (rhs.m_timing < lhs.m_timing) return false;
        return lhs.m_ltl < rhs.m_ltl;
    }

    explicit Requirement(const Formula& trigger, const Formula& response,
                         const Timing& timing)
        : m_trigger(trigger), m_response(response), m_timing(timing) {
        m_ltl = std::nullopt;
    }
};

static bool atom_contains_uppercase(const std::string& atom) {
    for (char c : atom) {
        if (c >= 'A' && c <= 'Z') {
            return true;
        }
    }
    return false;
}

struct Specification {
    std::set<Requirement> m_requirements;

    std::vector<std::string> m_in_atoms;
    std::vector<std::string> m_out_atoms;

    explicit Specification(std::set<Requirement> requirements = {},
                           std::vector<std::string> in_atoms = {},
                           std::vector<std::string> out_atoms = {})
        : m_requirements(std::move(requirements)),
          m_in_atoms(std::move(in_atoms)),
          m_out_atoms(std::move(out_atoms)) {
        for (const auto& atom : m_in_atoms) {
            if (atom_contains_uppercase(atom)) {
                std::cerr
                    << "[WARNING] in_atoms contains uppercase letter in atom '"
                    << atom << ", this can cause issues with Spot'\n";
            }
        }
        for (const auto& atom : m_out_atoms) {
            if (atom_contains_uppercase(atom)) {
                std::cerr
                    << "[WARNING] out_atoms contains uppercase letter in atom '"
                    << atom << ", this can cause issues with Spot'\n";
            }
        }
    }

    friend bool operator<(const Specification& lhs, const Specification& rhs) {
        return lhs.m_requirements < rhs.m_requirements;
    }
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
