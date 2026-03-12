#pragma once

#include <Eigen/Dense>
#include <cstdint>
#include <vector>

#include "requirement.hpp"

using Count = std::uint64_t;
using CountMatrix = Eigen::Matrix<Count, Eigen::Dynamic, Eigen::Dynamic>;
using CountVector = Eigen::Matrix<Count, Eigen::Dynamic, 1>;

struct TransferSystem {
    std::vector<State> states;
    CountVector valuation_counts;
    CountMatrix transition_matrix;
    bool transition_matrix_is_weighted = false;
};

TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts = CountVector());

CountMatrix weighted_transition_matrix(const TransferSystem& system);
