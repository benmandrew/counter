#include "fitness/transfer_matrix.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "fitness/ganak_runner.hpp"

namespace {

// Returns true when the requirement uses the countdown automata used for
// within-N-ticks and for-N-ticks semantics.
bool is_countdown_timing(const Timing& timing) {
    return std::holds_alternative<timing::WithinTicks>(timing) ||
           std::holds_alternative<timing::ForTicks>(timing);
}

std::size_t tick_count_or_throw(const Timing& timing) {
    if (const auto* within = std::get_if<timing::WithinTicks>(&timing)) {
        return within->m_ticks;
    }
    if (const auto* for_ticks = std::get_if<timing::ForTicks>(&timing)) {
        return for_ticks->m_ticks;
    }
    throw std::invalid_argument(
        "Tick count requested for non-countdown timing.");
}

CountVector counts_or_throw(const CountVector& provided_counts,
                            Eigen::Index expected_size,
                            const std::function<CountVector()>& loader,
                            const char* size_error_message);

bool is_valid_transition(const Requirement& requirement, const State& current,
                         const State& next);

std::vector<Eigen::Index> active_state_indices(
    const Requirement& requirement, const std::vector<State>& all_states);

std::vector<State> gather_states(const std::vector<State>& all_states,
                                 const std::vector<Eigen::Index>& indices);

// Uses supplied canonical valuation counts when provided, otherwise computes
// them via model counting, and validates the expected vector size.
CountVector canonical_valuation_counts_or_throw(
    const Requirement& requirement, const CountVector& provided_counts,
    Eigen::Index expected_size) {
    return counts_or_throw(
        provided_counts, expected_size,
        [&requirement]() {
            return count_canonical_valuation_counts(requirement);
        },
        "Expected one valuation count per canonical state.");
}

CountVector counts_or_throw(const CountVector& provided_counts,
                            Eigen::Index expected_size,
                            const std::function<CountVector()>& loader,
                            const char* size_error_message) {
    CountVector counts = provided_counts;
    if (counts.size() == 0) {
        counts = loader();
    }
    if (counts.size() != expected_size) {
        throw std::invalid_argument(size_error_message);
    }
    return counts;
}

// Builds the countdown state list c=0..N used by both countdown automata.
std::vector<State> countdown_states(std::size_t max_ticks) {
    std::vector<State> states;
    states.reserve(max_ticks + 1U);
    for (std::size_t tick = 0; tick <= max_ticks; ++tick) {
        states.push_back(State{false, false, true, tick});
    }
    return states;
}

// Computes the next countdown for [] (P -> <>[0,N] Q) without the dead state.
// Invalid transitions correspond to entering the omitted dead state.
std::size_t within_next_countdown(std::size_t countdown, bool trigger_holds,
                                  bool response_holds, std::size_t max_ticks,
                                  bool& valid) {
    if (response_holds) {
        valid = true;
        return 0;
    }
    if (countdown == 0) {
        valid = true;
        return trigger_holds ? max_ticks : 0;
    }
    if (countdown == 1) {
        valid = false;
        return 0;
    }
    if (trigger_holds) {
        valid = true;
        return max_ticks;
    }
    valid = true;
    return countdown - 1;
}

// Computes the next countdown for [] (P -> [][0,N] Q) without the dead state.
// Invalid transitions correspond to entering the omitted dead state.
std::size_t for_next_countdown(std::size_t countdown, bool trigger_holds,
                               bool response_holds, std::size_t max_ticks,
                               bool& valid) {
    if (!response_holds) {
        if (countdown > 0 || trigger_holds) {
            valid = false;
            return 0;
        }
        valid = true;
        return 0;
    }
    if (trigger_holds) {
        valid = true;
        return max_ticks;
    }
    if (countdown > 0) {
        valid = true;
        return countdown - 1;
    }
    valid = true;
    return 0;
}

using CountdownTransitionFn =
    std::function<std::size_t(std::size_t, bool, bool, std::size_t, bool&)>;

std::size_t canonical_cell_index(const State& valuation) {
    if (valuation.m_trigger_holds) {
        return valuation.m_response_holds ? 3U : 2U;
    }
    return valuation.m_response_holds ? 1U : 0U;
}

std::size_t joint_cell_index(std::size_t left_index, std::size_t right_index) {
    return left_index * 4U + right_index;
}

CountdownTransitionFn countdown_transition_fn_or_throw(
    const Requirement& requirement);

std::string valuation_formula(const Requirement& requirement,
                              const State& valuation);

struct RequirementAutomaton {
    Requirement m_requirement;
    std::vector<State> m_states;
    std::array<Eigen::Index, 4> m_cell_to_state = {-1, -1, -1, -1};
    CountdownTransitionFn m_countdown_transition;

    explicit RequirementAutomaton(const Requirement& requirement)
        : m_requirement(requirement) {
        this->m_requirement = requirement;
        if (is_countdown_timing(requirement.m_timing)) {
            this->m_states =
                countdown_states(tick_count_or_throw(requirement.m_timing));
            this->m_countdown_transition =
                countdown_transition_fn_or_throw(requirement);
            return;
        }
        const std::vector<State> cells = canonical_states();
        const std::vector<Eigen::Index> active_indices =
            active_state_indices(requirement, cells);
        this->m_states = gather_states(cells, active_indices);
        for (Eigen::Index state_index = 0;
             state_index < static_cast<Eigen::Index>(this->m_states.size());
             ++state_index) {
            const std::size_t index = canonical_cell_index(
                this->m_states[static_cast<std::size_t>(state_index)]);
            this->m_cell_to_state[index] = state_index;
        }
        if (std::holds_alternative<timing::NextTimepoint>(
                requirement.m_timing) &&
            this->m_states.size() != cells.size()) {
            throw std::logic_error(
                "Failed to build next-timepoint automaton states.");
        }
    }
};

CountVector joint_valuation_counts_or_throw(
    const Requirement& requirement1, const Requirement& requirement2,
    const CountVector& provided_counts) {
    return counts_or_throw(
        provided_counts, 16,
        [&requirement1, &requirement2]() {
            return count_joint_valuation_counts(requirement1, requirement2);
        },
        "Expected one valuation count per joint canonical cell.");
}

bool next_state_from_cell(const RequirementAutomaton& automaton,
                          Eigen::Index current_state_index,
                          const State& cell_valuation,
                          Eigen::Index& next_state_index) {
    if (is_countdown_timing(automaton.m_requirement.m_timing)) {
        bool valid_transition = false;
        const std::size_t next_countdown = automaton.m_countdown_transition(
            static_cast<std::size_t>(current_state_index),
            cell_valuation.m_trigger_holds, cell_valuation.m_response_holds,
            tick_count_or_throw(automaton.m_requirement.m_timing),
            valid_transition);
        if (!valid_transition) {
            return false;
        }
        next_state_index = static_cast<Eigen::Index>(next_countdown);
        return true;
    }
    const Eigen::Index destination =
        automaton.m_cell_to_state[canonical_cell_index(cell_valuation)];
    if (destination < 0) {
        return false;
    }
    const State& current =
        automaton.m_states[static_cast<std::size_t>(current_state_index)];
    const State& next =
        automaton.m_states[static_cast<std::size_t>(destination)];
    if (!is_valid_transition(automaton.m_requirement, current, next)) {
        return false;
    }
    next_state_index = destination;
    return true;
}

std::string joint_valuation_formula(const Requirement& requirement1,
                                    const Requirement& requirement2,
                                    const State& valuation1,
                                    const State& valuation2) {
    return "(" + valuation_formula(requirement1, valuation1) + ") & (" +
           valuation_formula(requirement2, valuation2) + ")";
}

// Selects the countdown transition function for the requested timing.
CountdownTransitionFn countdown_transition_fn_or_throw(
    const Requirement& requirement) {
    if (std::holds_alternative<timing::WithinTicks>(requirement.m_timing)) {
        return &within_next_countdown;
    }
    if (std::holds_alternative<timing::ForTicks>(requirement.m_timing)) {
        return &for_next_countdown;
    }
    throw std::invalid_argument("Invalid timing for countdown automaton.");
}

// Constructs the weighted live-state transition matrix by aggregating all four
// canonical valuation cells and skipping dead-state transitions.
CountMatrix build_countdown_weighted_transitions(
    CountdownTransitionFn transition_fn, std::size_t max_ticks,
    const std::vector<State>& cells, const CountVector& cell_counts) {
    const Eigen::Index state_count = static_cast<Eigen::Index>(max_ticks + 1U);
    CountMatrix weighted_transitions(state_count, state_count);
    weighted_transitions.setZero();
    for (Eigen::Index row = 0; row < state_count; ++row) {
        const std::size_t countdown = static_cast<std::size_t>(row);
        for (Eigen::Index cell_index = 0;
             cell_index < static_cast<Eigen::Index>(cells.size());
             ++cell_index) {
            const State& cell = cells[static_cast<std::size_t>(cell_index)];
            bool valid_transition = false;
            const std::size_t next_countdown = transition_fn(
                countdown, cell.m_trigger_holds, cell.m_response_holds,
                max_ticks, valid_transition);
            if (!valid_transition) {
                continue;
            }
            const Eigen::Index column =
                static_cast<Eigen::Index>(next_countdown);
            Count updated = 0;
            if (count_add_overflow(weighted_transitions(row, column),
                                   cell_counts(cell_index), updated)) {
                throw std::overflow_error(
                    "Count overflow while building weighted transitions.");
            }
            weighted_transitions(row, column) = updated;
        }
    }
    return weighted_transitions;
}

// Computes the initial state distribution for trace counting from c=0.
CountVector countdown_initial_counts(const CountMatrix& weighted_transitions) {
    return weighted_transitions.row(0).transpose();
}

// Returns true when a canonical valuation satisfies the state invariant.
bool is_valid_state(const Requirement& requirement, const State& state) {
    if (std::holds_alternative<timing::Immediately>(requirement.m_timing)) {
        return !state.m_trigger_holds || state.m_response_holds;
    }
    return true;
}

// Returns true when a transition between live states is allowed by timing.
bool is_valid_transition(const Requirement& requirement, const State& current,
                         const State& next) {
    if (std::holds_alternative<timing::NextTimepoint>(requirement.m_timing)) {
        return !current.m_trigger_holds || next.m_response_holds;
    }
    return true;
}

// Extracts canonical state indices that remain live under the requirement.
std::vector<Eigen::Index> active_state_indices(
    const Requirement& requirement, const std::vector<State>& all_states) {
    std::vector<Eigen::Index> indices;
    for (Eigen::Index index = 0;
         index < static_cast<Eigen::Index>(all_states.size()); ++index) {
        if (is_valid_state(requirement,
                           all_states[static_cast<std::size_t>(index)])) {
            indices.push_back(index);
        }
    }
    return indices;
}

// Materializes live-state descriptors from selected canonical indices.
std::vector<State> gather_states(const std::vector<State>& all_states,
                                 const std::vector<Eigen::Index>& indices) {
    std::vector<State> states;
    states.reserve(indices.size());
    for (Eigen::Index index : indices) {
        states.push_back(all_states[static_cast<std::size_t>(index)]);
    }
    return states;
}

// Materializes valuation counts aligned with the selected active indices.
CountVector gather_counts(const CountVector& all_counts,
                          const std::vector<Eigen::Index>& indices) {
    CountVector counts(static_cast<Eigen::Index>(indices.size()));
    for (Eigen::Index index = 0;
         index < static_cast<Eigen::Index>(indices.size()); ++index) {
        counts(index) = all_counts(indices[static_cast<std::size_t>(index)]);
    }
    return counts;
}

// Builds the unweighted adjacency matrix over active states.
CountMatrix build_unweighted_transition_matrix(
    const Requirement& requirement, const std::vector<State>& active_states) {
    const Eigen::Index state_count =
        static_cast<Eigen::Index>(active_states.size());
    CountMatrix transition_matrix(state_count, state_count);
    transition_matrix.setZero();
    for (Eigen::Index row = 0; row < state_count; ++row) {
        const State& current = active_states[static_cast<std::size_t>(row)];
        for (Eigen::Index column = 0; column < state_count; ++column) {
            const State& next = active_states[static_cast<std::size_t>(column)];
            if (is_valid_transition(requirement, current, next)) {
                transition_matrix(row, column) = 1;
            }
        }
    }
    return transition_matrix;
}

// Builds a valuation formula fixing trigger/response truth values for one
// canonical valuation cell. Note: Formula concatenation is approximate and
// may not produce a valid formula if subformulae contain operators. For
// model counting, the actual propositional conditions should be evaluated
// against specific variable assignments.
std::string valuation_formula(const Requirement& requirement,
                              const State& valuation) {
    const std::string trigger_str = requirement.m_trigger.to_string();
    const std::string response_str = requirement.m_response.to_string();
    const std::string trigger_clause =
        valuation.m_trigger_holds ? trigger_str : "!(" + trigger_str + ")";
    const std::string response_clause =
        valuation.m_response_holds ? response_str : "!(" + response_str + ")";
    return "(" + trigger_clause + ") & (" + response_clause + ")";
}

// Builds the countdown transfer system for within-N-ticks and for-N-ticks
// semantics using a live-state matrix (dead state omitted).
TransferSystem build_countdown_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts) {
    const CountdownTransitionFn transition_fn =
        countdown_transition_fn_or_throw(requirement);
    const std::vector<State> cells = canonical_states();
    const CountVector cell_counts = canonical_valuation_counts_or_throw(
        requirement, canonical_valuation_counts,
        static_cast<Eigen::Index>(cells.size()));
    const std::size_t max_ticks = tick_count_or_throw(requirement.m_timing);
    const std::vector<State> states = countdown_states(max_ticks);
    const CountMatrix weighted_transitions =
        build_countdown_weighted_transitions(transition_fn, max_ticks, cells,
                                             cell_counts);
    const CountVector initial_counts =
        countdown_initial_counts(weighted_transitions);
    return {states, initial_counts, weighted_transitions, true};
}

}  // namespace

// Counts the number of full-model valuations for each canonical trigger/
// response cell using Ganak.
CountVector count_canonical_valuation_counts(const Requirement& requirement,
                                             unsigned seed) {
    const std::vector<State> valuations = canonical_states();
    CountVector counts(static_cast<Eigen::Index>(valuations.size()));
    for (Eigen::Index index = 0;
         index < static_cast<Eigen::Index>(valuations.size()); ++index) {
        counts(index) = run_ganak_on_formula(
            valuation_formula(requirement,
                              valuations[static_cast<std::size_t>(index)]),
            seed);
    }
    return counts;
}

CountVector count_joint_valuation_counts(const Requirement& requirement1,
                                         const Requirement& requirement2,
                                         unsigned seed) {
    const std::vector<State> cells = canonical_states();
    CountVector counts(16);
    Eigen::Index count_index = 0;
    for (const State& left_cell : cells) {
        for (const State& right_cell : cells) {
            counts(count_index) = run_ganak_on_formula(
                joint_valuation_formula(requirement1, requirement2, left_cell,
                                        right_cell),
                seed);
            ++count_index;
        }
    }
    return counts;
}

// Builds the transfer system for all supported timings. Countdown timings are
// built directly as weighted live-state systems; others are unweighted and are
// weighted later via valuation counts.
TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts) {
    if (is_countdown_timing(requirement.m_timing)) {
        return build_countdown_transfer_system(requirement,
                                               canonical_valuation_counts);
    }
    const std::vector<State> all_states = canonical_states();
    const CountVector all_counts = canonical_valuation_counts_or_throw(
        requirement, canonical_valuation_counts,
        static_cast<Eigen::Index>(all_states.size()));
    const std::vector<Eigen::Index> active_indices =
        active_state_indices(requirement, all_states);
    const std::vector<State> active_states =
        gather_states(all_states, active_indices);
    const CountVector active_counts = gather_counts(all_counts, active_indices);
    const CountMatrix transition_matrix =
        build_unweighted_transition_matrix(requirement, active_states);
    return {active_states, active_counts, transition_matrix, false};
}

// Returns a weighted transition matrix. If already weighted, this is a
// pass-through; otherwise each destination column is multiplied by its
// valuation count.
CountMatrix weighted_transition_matrix(const TransferSystem& system) {
    if (system.m_transition_matrix_is_weighted) {
        return system.m_transition_matrix;
    }
    CountMatrix weighted = system.m_transition_matrix;
    for (Eigen::Index column = 0; column < weighted.cols(); ++column) {
        for (Eigen::Index row = 0; row < weighted.rows(); ++row) {
            Count updated = 0;
            if (count_mul_overflow(weighted(row, column),
                                   system.m_valuation_counts(column),
                                   updated)) {
                throw std::overflow_error(
                    "Count overflow while weighting transition matrix.");
            }
            weighted(row, column) = updated;
        }
    }
    return weighted;
}

CountMatrix build_combined_weighted_transition_matrix(
    const Requirement& requirement1, const Requirement& requirement2,
    const CountVector& joint_valuation_counts) {
    const RequirementAutomaton left(requirement1);
    const RequirementAutomaton right(requirement2);
    const CountVector cell_counts = joint_valuation_counts_or_throw(
        requirement1, requirement2, joint_valuation_counts);
    const std::vector<State> cells = canonical_states();
    const Eigen::Index left_state_count =
        static_cast<Eigen::Index>(left.m_states.size());
    const Eigen::Index right_state_count =
        static_cast<Eigen::Index>(right.m_states.size());
    const Eigen::Index state_count = left_state_count * right_state_count;
    CountMatrix weighted(state_count, state_count);
    weighted.setZero();
    for (Eigen::Index left_row = 0; left_row < left_state_count; ++left_row) {
        for (Eigen::Index right_row = 0; right_row < right_state_count;
             ++right_row) {
            const Eigen::Index row = left_row * right_state_count + right_row;
            for (std::size_t left_cell_index = 0; left_cell_index < 4U;
                 ++left_cell_index) {
                const State& left_cell = cells[left_cell_index];
                Eigen::Index left_next = 0;
                if (!next_state_from_cell(left, left_row, left_cell,
                                          left_next)) {
                    continue;
                }
                for (std::size_t right_cell_index = 0; right_cell_index < 4U;
                     ++right_cell_index) {
                    const State& right_cell = cells[right_cell_index];
                    Eigen::Index right_next = 0;
                    if (!next_state_from_cell(right, right_row, right_cell,
                                              right_next)) {
                        continue;
                    }
                    const std::size_t cell_index =
                        joint_cell_index(left_cell_index, right_cell_index);
                    const Count weight =
                        cell_counts(static_cast<Eigen::Index>(cell_index));
                    if (weight == 0) {
                        continue;
                    }
                    const Eigen::Index column =
                        left_next * right_state_count + right_next;
                    Count updated = 0;
                    if (count_add_overflow(weighted(row, column), weight,
                                           updated)) {
                        throw std::overflow_error(
                            "Count overflow while building combined weighted "
                            "transfer matrix.");
                    }
                    weighted(row, column) = updated;
                }
            }
        }
    }

    return weighted;
}
