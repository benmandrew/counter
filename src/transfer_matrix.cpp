#include "transfer_matrix.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

CountVector default_valuation_counts(Eigen::Index state_count) {
    CountVector counts(state_count);
    counts.setOnes();
    return counts;
}

bool is_valid_state(const Requirement& requirement, const State& state) {
    if (requirement.timing == Timing::Immediately) {
        return !state.trigger_holds || state.response_holds;
    }

    return true;
}

bool is_valid_transition(const Requirement& requirement, const State& current,
                         const State& next) {
    if (requirement.timing == Timing::NextTimepoint) {
        return !current.trigger_holds || next.response_holds;
    }

    return true;
}

std::size_t within_next_countdown(std::size_t countdown, bool trigger_holds,
                                  bool response_holds, std::size_t max_ticks,
                                  bool* valid) {
    if (response_holds) {
        *valid = true;
        return 0;
    }

    if (countdown == 0) {
        *valid = true;
        return trigger_holds ? max_ticks : 0;
    }

    if (countdown == 1) {
        *valid = false;
        return 0;
    }

    if (trigger_holds) {
        *valid = true;
        return max_ticks;
    }

    *valid = true;
    return countdown - 1;
}

std::size_t for_next_countdown(std::size_t countdown, bool trigger_holds,
                               bool response_holds, std::size_t max_ticks,
                               bool* valid) {
    if (!response_holds) {
        if (countdown > 0 || trigger_holds) {
            *valid = false;
            return 0;
        }

        *valid = true;
        return 0;
    }

    if (trigger_holds) {
        *valid = true;
        return max_ticks;
    }

    if (countdown > 0) {
        *valid = true;
        return countdown - 1;
    }

    *valid = true;
    return 0;
}

TransferSystem build_countdown_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts) {
    if (requirement.timing != Timing::WithinTicks &&
        requirement.timing != Timing::ForTicks) {
        throw std::invalid_argument("Invalid timing for countdown automaton.");
    }

    const std::vector<State> cells = canonical_states();
    CountVector cell_counts = canonical_valuation_counts;
    if (cell_counts.size() == 0) {
        cell_counts =
            default_valuation_counts(static_cast<Eigen::Index>(cells.size()));
    }

    if (cell_counts.size() != static_cast<Eigen::Index>(cells.size())) {
        throw std::invalid_argument(
            "Expected one valuation count per canonical state.");
    }

    const std::size_t max_ticks = requirement.tick_count;
    const Eigen::Index state_count = static_cast<Eigen::Index>(max_ticks + 1U);

    std::vector<State> countdown_states;
    countdown_states.reserve(static_cast<std::size_t>(state_count));
    for (std::size_t tick = 0; tick <= max_ticks; ++tick) {
        countdown_states.push_back(
            State{false, false, true, static_cast<std::size_t>(tick)});
    }

    CountMatrix weighted_transitions(state_count, state_count);
    weighted_transitions.setZero();

    for (Eigen::Index row = 0; row < state_count; ++row) {
        const std::size_t countdown = static_cast<std::size_t>(row);

        for (Eigen::Index cell_index = 0;
             cell_index < static_cast<Eigen::Index>(cells.size());
             ++cell_index) {
            const State& cell = cells[static_cast<std::size_t>(cell_index)];
            bool valid_transition = false;
            std::size_t next_countdown = 0;

            if (requirement.timing == Timing::WithinTicks) {
                next_countdown = within_next_countdown(
                    countdown, cell.trigger_holds, cell.response_holds,
                    max_ticks, &valid_transition);
            } else {
                next_countdown = for_next_countdown(
                    countdown, cell.trigger_holds, cell.response_holds,
                    max_ticks, &valid_transition);
            }

            if (!valid_transition) {
                continue;
            }

            const Eigen::Index column =
                static_cast<Eigen::Index>(next_countdown);
            weighted_transitions(row, column) += cell_counts(cell_index);
        }
    }

    CountVector initial_counts(state_count);
    initial_counts = weighted_transitions.row(0).transpose();

    return {countdown_states, initial_counts, weighted_transitions, true};
}

TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts) {
    if (requirement.timing == Timing::WithinTicks ||
        requirement.timing == Timing::ForTicks) {
        return build_countdown_transfer_system(requirement,
                                               canonical_valuation_counts);
    }

    const std::vector<State> all_states = canonical_states();

    CountVector all_counts = canonical_valuation_counts;
    if (all_counts.size() == 0) {
        all_counts = default_valuation_counts(
            static_cast<Eigen::Index>(all_states.size()));
    }

    if (all_counts.size() != static_cast<Eigen::Index>(all_states.size())) {
        throw std::invalid_argument(
            "Expected one valuation count per canonical state.");
    }

    std::vector<Eigen::Index> active_indices;
    std::vector<State> active_states;
    for (Eigen::Index index = 0;
         index < static_cast<Eigen::Index>(all_states.size()); ++index) {
        if (!is_valid_state(requirement,
                            all_states[static_cast<std::size_t>(index)])) {
            continue;
        }

        active_indices.push_back(index);
        active_states.push_back(all_states[static_cast<std::size_t>(index)]);
    }

    CountVector active_counts(static_cast<Eigen::Index>(active_indices.size()));
    for (Eigen::Index index = 0;
         index < static_cast<Eigen::Index>(active_indices.size()); ++index) {
        active_counts(index) =
            all_counts(active_indices[static_cast<std::size_t>(index)]);
    }

    const Eigen::Index state_count =
        static_cast<Eigen::Index>(active_indices.size());
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

    return {active_states, active_counts, transition_matrix, false};
}

CountMatrix weighted_transition_matrix(const TransferSystem& system) {
    if (system.transition_matrix_is_weighted) {
        return system.transition_matrix;
    }

    CountMatrix weighted = system.transition_matrix;
    for (Eigen::Index column = 0; column < weighted.cols(); ++column) {
        weighted.col(column) *= system.valuation_counts(column);
    }
    return weighted;
}
