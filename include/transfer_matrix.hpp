#pragma once

#include <Eigen/Dense>
#include <cstdint>
#include <vector>

#include "requirement.hpp"

using Count = std::uint64_t;
using CountMatrix = Eigen::Matrix<Count, Eigen::Dynamic, Eigen::Dynamic>;
using CountVector = Eigen::Matrix<Count, Eigen::Dynamic, 1>;

struct TransferSystem {
    std::vector<State> m_states;
    CountVector m_valuation_counts;
    CountMatrix m_transition_matrix;
    bool m_transition_matrix_is_weighted = false;
};

TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts = CountVector());

CountVector count_canonical_valuation_counts(const Requirement& requirement,
                                             unsigned seed = 1);

CountMatrix weighted_transition_matrix(const TransferSystem& system);
