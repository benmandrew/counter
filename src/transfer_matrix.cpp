#include "transfer_matrix.hpp"

// Returns a weighted transition matrix. If already weighted, this is a
// pass-through; otherwise each destination column is multiplied by its
// valuation count.
CountMatrix weighted_transition_matrix(const TransferSystem& system) {
    if (system.m_transition_matrix_is_weighted) {
        return system.m_transition_matrix;
    }
    CountMatrix weighted = system.m_transition_matrix;
    for (Eigen::Index column = 0; column < weighted.cols(); ++column) {
        weighted.col(column) *= system.m_valuation_counts(column);
    }
    return weighted;
}
