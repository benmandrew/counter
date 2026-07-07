#pragma once

/// @file requirement.hpp
/// @brief Core domain types: Timing, ConditionType, Requirement, Specification,
///        and the automaton State used for model counting.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "prop_formula.hpp"

namespace timing {

/// Response must hold immediately when condition is true.
struct Immediately {};

/// Response must hold at the next timepoint after condition.
struct NextTimepoint {};

/// Response must hold within `m_ticks` ticks of condition, including the
/// condition tick.
struct WithinTicks {
    std::size_t m_ticks;
};

/// Response must hold for `m_ticks` consecutive ticks, including the condition
/// tick.
struct ForTicks {
    std::size_t m_ticks;
};

/// Response must not hold for `m_ticks` + 1 ticks starting from the condition
/// tick (i.e., at t=0,...,m_ticks), then must hold at t=m_ticks+1.
/// Implements FRET: after m_ticks res = (for m_ticks ¬res) ∧ (within
/// (m_ticks+1) res).
struct AfterTicks {
    std::size_t m_ticks;
};

/// Response must hold at some timepoint at or after the condition.
struct Eventually {};

/// Algebraic data type for requirement timing.
using Timing = std::variant<Immediately, NextTimepoint, WithinTicks, ForTicks,
                            AfterTicks, Eventually>;

inline Timing immediately() { return Immediately{}; }
inline Timing next_timepoint() { return NextTimepoint{}; }
inline Timing within_ticks(std::size_t ticks) { return WithinTicks{ticks}; }
inline Timing for_ticks(std::size_t ticks) { return ForTicks{ticks}; }
inline Timing after_ticks(std::size_t ticks) { return AfterTicks{ticks}; }
inline Timing eventually() { return Eventually{}; }

}  // namespace timing

using Timing = timing::Timing;

bool operator<(const Timing& lhs, const Timing& rhs);
bool operator==(const Timing& lhs, const Timing& rhs);

/// Distinguishes how the condition field activates the requirement.
/// - Trigger: requirement fires on a rising edge of the condition (false→true),
///   or if the condition already holds at t=0.
/// - Continual: requirement fires at every timepoint where the condition holds.
enum class ConditionType : std::uint8_t { Trigger, Continual };

/// A FRET requirement specifying a system obligation. Consists of a condition
/// and a response that must be satisfied according to the specified timing
/// constraint. These are used as the basic units for repair and for computing
/// semantic and syntactic similarity metrics in genetic algorithms.
struct Requirement {
    /// The condition (propositional formula) that activates the requirement
    Formula m_condition;
    /// The response obligation (propositional formula)
    Formula m_response;
    /// The timing constraint for satisfaction
    Timing m_timing;
    /// Whether the condition is evaluated as a trigger (rising-edge) or
    /// continually (at every timepoint where it holds)
    ConditionType m_condition_type;
    /// The LTL formula equivalent to (m_condition, m_response, m_timing,
    /// m_condition_type), derived automatically by the constructor via
    /// requirement_to_ltl.
    std::string m_ltl;

    friend bool operator<(const Requirement& lhs, const Requirement& rhs);
    friend bool operator==(const Requirement& lhs, const Requirement& rhs);

    explicit Requirement(
        Formula condition, Formula response, const Timing& timing,
        ConditionType condition_type = ConditionType::Continual);

    /// Returns a one-line FRETish string of the form
    /// "[upon|whenever <condition>] C shall <timing> satisfy <response>",
    /// parseable by the FRET formaliser CLI (see runner/formaliser.hpp).
    [[nodiscard]] std::string to_string() const;

   private:
    [[nodiscard]] std::string condition_to_string() const;
};

struct Specification {
    std::vector<Requirement> m_assumptions;
    std::vector<Requirement> m_guarantees;

    std::vector<std::string> m_in_atoms;
    std::vector<std::string> m_out_atoms;

    explicit Specification(std::vector<Requirement> assumptions = {},
                           std::vector<Requirement> guarantees = {},
                           std::vector<std::string> in_atoms = {},
                           std::vector<std::string> out_atoms = {});

    friend bool operator<(const Specification& lhs, const Specification& rhs);
    friend bool operator==(const Specification& lhs, const Specification& rhs);

    /// Returns one FRETish line per requirement (assumptions then
    /// guarantees), each as produced by Requirement::to_string, joined by
    /// newlines.
    [[nodiscard]] std::string to_string() const;
};

/// A state in the automaton used for transfer matrix model counting. Encodes
/// the state of a requirement automaton at a particular timepoint, used to
/// compute both individual and joint requirement model counts.
struct State {
    /// Whether the condition holds
    bool m_condition_holds = false;
    /// Whether the response condition holds
    bool m_response_holds = false;
    /// Countdown mechanism state (for timed constraints)
    bool m_countdown_state = false;
    /// Remaining ticks in countdown
    std::size_t m_countdown_ticks = 0;

    /// Returns a human-readable label for this state (used for debugging).
    [[nodiscard]] std::string label() const;
};

/// Returns the set of canonical states based on the cross-product of condition
/// and response boolean valuations: {(C,R) | C,R ∈ {true, false}}.
std::vector<State> canonical_states();

/// Returns true if any assumption or guarantee has a condition that is the
/// literal atom "false" (e.g. after simplifying "!(true)"). Such a
/// requirement is vacuously satisfied by every trace and imposes no
/// constraint, so specifications containing one should be excluded from the
/// population rather than treated as ordinary candidates.
bool specification_has_false_condition(const Specification& specification);

/// Converts a Timing enum value to a human-readable string representation.
std::string to_string(const Timing& timing);

/// Converts a Requirement to an LTL formula string in SPOT syntax. For
/// Continual condition type, bounded timing variants are expanded into X (next)
/// operator chains: WithinTicks(n) yields G(C -> (R | X(R | ... | X R))) and
/// ForTicks(n) yields G(C -> (R & X(R & ... & X R))). For Trigger condition
/// type, the formula encodes a rising-edge detector:
/// G((!C & X(C)) -> X(body)) & (C -> body).
/// The result is suitable for passing directly to ltl2tgba or ltlsynt.
std::string requirement_to_ltl(const Requirement& requirement);

/// \cond
namespace std {  // NOLINT(build/namespaces)

template <>
struct hash<Timing> {
    std::size_t operator()(const Timing& timing) const noexcept {
        auto combine = [](std::size_t seed, std::size_t val) noexcept {
            return seed ^ (val + 0x9e3779b9U + (seed << 6) + (seed >> 2));
        };
        return std::visit(
            [&combine, idx = timing.index()](const auto& val) -> std::size_t {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, timing::WithinTicks> ||
                              std::is_same_v<T, timing::ForTicks> ||
                              std::is_same_v<T, timing::AfterTicks>) {
                    return combine(idx, std::hash<std::size_t>{}(val.m_ticks));
                } else {
                    return idx;
                }
            },
            timing);
    }
};

template <>
struct hash<Requirement> {
    std::size_t operator()(const Requirement& req) const noexcept {
        auto combine = [](std::size_t seed, std::size_t val) noexcept {
            return seed ^ (val + 0x9e3779b9U + (seed << 6) + (seed >> 2));
        };
        std::size_t seed = std::hash<Formula>{}(req.m_condition);
        seed = combine(seed, std::hash<Formula>{}(req.m_response));
        seed = combine(seed, std::hash<Timing>{}(req.m_timing));
        seed = combine(seed, std::hash<bool>{}(req.m_condition_type ==
                                               ConditionType::Trigger));
        seed = combine(seed, std::hash<std::string>{}(req.m_ltl));
        return seed;
    }
};

template <>
struct hash<Specification> {
    std::size_t operator()(const Specification& spec) const noexcept {
        auto combine = [](std::size_t seed, std::size_t val) noexcept {
            return seed ^ (val + 0x9e3779b9U + (seed << 6) + (seed >> 2));
        };
        std::size_t seed = 0;
        for (const Requirement& req : spec.m_assumptions) {
            seed = combine(seed, std::hash<Requirement>{}(req));
        }
        for (const Requirement& req : spec.m_guarantees) {
            seed = combine(seed, std::hash<Requirement>{}(req));
        }
        for (const std::string& atom : spec.m_in_atoms) {
            seed = combine(seed, std::hash<std::string>{}(atom));
        }
        for (const std::string& atom : spec.m_out_atoms) {
            seed = combine(seed, std::hash<std::string>{}(atom));
        }
        return seed;
    }
};

}  // namespace std
/// \endcond
