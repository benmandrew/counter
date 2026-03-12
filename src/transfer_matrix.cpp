#include "transfer_matrix.hpp"

#include <stdexcept>
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

TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts) {
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

    return {active_states, active_counts, transition_matrix};
}

CountMatrix weighted_transition_matrix(const TransferSystem& system) {
    CountMatrix weighted = system.transition_matrix;
    for (Eigen::Index column = 0; column < weighted.cols(); ++column) {
        weighted.col(column) *= system.valuation_counts(column);
    }
    return weighted;
}
