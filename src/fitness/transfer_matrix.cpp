#include "fitness/transfer_matrix.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "runner/ganak.hpp"

namespace {

// Returns true when the requirement uses the countdown automata used for
// within-N-ticks, for-N-ticks, and after-N-ticks semantics.
bool is_countdown_timing(const Timing& timing) {
    return std::holds_alternative<timing::WithinTicks>(timing) ||
           std::holds_alternative<timing::ForTicks>(timing) ||
           std::holds_alternative<timing::AfterTicks>(timing);
}

std::size_t tick_count_or_throw(const Timing& timing) {
    if (const auto* within = std::get_if<timing::WithinTicks>(&timing)) {
        return within->m_ticks;
    }
    if (const auto* after = std::get_if<timing::AfterTicks>(&timing)) {
        return after->m_ticks;
    }
    const auto* for_ticks = std::get_if<timing::ForTicks>(&timing);
    assert(for_ticks != nullptr);
    return for_ticks->m_ticks;
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
                            [[maybe_unused]] Eigen::Index expected_size,
                            const std::function<CountVector()>& loader,
                            const char* /*size_error_message*/) {
    CountVector counts = provided_counts;
    if (counts.size() == 0) {
        counts = loader();
    }
    assert(counts.size() == expected_size);
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

// Computes the next countdown for [] (P -> after[N] Q).
// States: c=0 (idle), c=1 (required: Q must hold), c=2..N (forbidden: Q must
// not hold). Trigger fires at c=0 with Q absent -> c=N. Invalid transitions
// correspond to entering the omitted dead state.
std::size_t after_next_countdown(std::size_t countdown, bool trigger_holds,
                                 bool response_holds, std::size_t max_ticks,
                                 bool& valid) {
    if (countdown == 0) {
        if (!trigger_holds) {
            valid = true;
            return 0;
        }
        if (response_holds) {
            valid = false;
            return 0;
        }
        valid = true;
        return max_ticks;
    }
    if (countdown == 1) {
        valid = response_holds && !trigger_holds;
        return 0;
    }
    // countdown >= 2: forbidden tick
    if (response_holds) {
        valid = false;
        return 0;
    }
    valid = true;
    return trigger_holds ? max_ticks : countdown - 1;
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
        if (std::holds_alternative<timing::Eventually>(requirement.m_timing)) {
            // 2 states: index 0 = not-pending, index 1 = pending
            this->m_states = {State{false, false, false, 0},
                              State{false, false, false, 0}};
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
        assert(!std::holds_alternative<timing::NextTimepoint>(
                   requirement.m_timing) ||
               this->m_states.size() == cells.size());
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
    if (std::holds_alternative<timing::Eventually>(
            automaton.m_requirement.m_timing)) {
        // pending' = (pending || trigger) && !response
        const bool pending = (current_state_index == 1);
        const bool new_pending = (pending || cell_valuation.m_trigger_holds) &&
                                 !cell_valuation.m_response_holds;
        next_state_index = new_pending ? 1 : 0;
        return true;
    }
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
    if (std::holds_alternative<timing::AfterTicks>(requirement.m_timing)) {
        return &after_next_countdown;
    }
    assert(std::holds_alternative<timing::ForTicks>(requirement.m_timing));
    return &for_next_countdown;
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
            [[maybe_unused]] const bool overflow =
                count_add_overflow(weighted_transitions(row, column),
                                   cell_counts(cell_index), updated);
            assert(!overflow);
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
    return {states, initial_counts, weighted_transitions, true, {}};
}

// Builds the transfer system for G(trigger -> F response).
// States: {0: not-pending, 1: pending}. Only state 0 is a valid final state.
TransferSystem build_eventually_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts) {
    const std::vector<State> cells = canonical_states();
    const CountVector cell_counts = canonical_valuation_counts_or_throw(
        requirement, canonical_valuation_counts,
        static_cast<Eigen::Index>(cells.size()));
    // c[0]=¬T∧¬R  c[1]=¬T∧R  c[2]=T∧¬R  c[3]=T∧R
    const Count c00 = cell_counts(0);
    const Count c01 = cell_counts(1);
    const Count c10 = cell_counts(2);
    const Count c11 = cell_counts(3);
    CountMatrix m(2, 2);
    // From not-pending (0): go to not-pending unless (T∧¬R)
    Count r00 = 0;
    [[maybe_unused]] bool ov = false;
    ov = count_add_overflow(c00, c01, r00);
    assert(!ov);
    ov = count_add_overflow(r00, c11, r00);
    assert(!ov);
    m(0, 0) = r00;
    m(0, 1) = c10;
    // From pending (1): go to not-pending when response holds
    Count r10 = 0;
    ov = count_add_overflow(c01, c11, r10);
    assert(!ov);
    m(1, 0) = r10;
    Count r11 = 0;
    ov = count_add_overflow(c00, c10, r11);
    assert(!ov);
    m(1, 1) = r11;
    const std::vector<State> states = {State{false, false, false, 0},
                                       State{false, false, false, 0}};
    CountVector final_mask(2);
    final_mask(0) = 1;
    final_mask(1) = 0;
    CountVector initial_counts(2);
    initial_counts(0) = m(0, 0) + m(0, 1);
    initial_counts(1) = m(1, 0) + m(1, 1);
    return {states, initial_counts, m, true, final_mask};
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
    if (std::holds_alternative<timing::Eventually>(requirement.m_timing)) {
        return build_eventually_transfer_system(requirement,
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
    return {active_states, active_counts, transition_matrix, false, {}};
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
            [[maybe_unused]] const bool overflow =
                count_mul_overflow(weighted(row, column),
                                   system.m_valuation_counts(column), updated);
            assert(!overflow);
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
                    [[maybe_unused]] const bool overflow = count_add_overflow(
                        weighted(row, column), weight, updated);
                    assert(!overflow);
                    weighted(row, column) = updated;
                }
            }
        }
    }

    return weighted;
}

CountVector build_combined_final_state_mask(const Requirement& requirement1,
                                            const Requirement& requirement2) {
    const bool left_eventually =
        std::holds_alternative<timing::Eventually>(requirement1.m_timing);
    const bool right_eventually =
        std::holds_alternative<timing::Eventually>(requirement2.m_timing);
    if (!left_eventually && !right_eventually) {
        return {};
    }
    const RequirementAutomaton left(requirement1);
    const RequirementAutomaton right(requirement2);
    const std::size_t n_left = left.m_states.size();
    const std::size_t n_right = right.m_states.size();
    CountVector mask(static_cast<Eigen::Index>(n_left * n_right));
    for (std::size_t i = 0; i < n_left; ++i) {
        const bool left_valid = !left_eventually || (i == 0);
        for (std::size_t j = 0; j < n_right; ++j) {
            const bool right_valid = !right_eventually || (j == 0);
            mask(static_cast<Eigen::Index>(i * n_right + j)) =
                (left_valid && right_valid) ? 1 : 0;
        }
    }
    return mask;
}
