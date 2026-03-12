#include "model_counter.hpp"

#include <stdexcept>

CountMatrix matrix_power(const CountMatrix& matrix, std::size_t exponent) {
    if (matrix.rows() != matrix.cols()) {
        throw std::invalid_argument("Transfer matrices must be square.");
    }

    CountMatrix result = CountMatrix::Identity(matrix.rows(), matrix.cols());
    CountMatrix factor = matrix;

    while (exponent > 0) {
        if ((exponent & 1U) != 0U) {
            result = result * factor;
        }

        exponent >>= 1U;
        if (exponent > 0) {
            factor = factor * factor;
        }
    }

    return result;
}

Count count_traces(const TransferSystem& system, std::size_t trace_length) {
    if (trace_length == 0) {
        throw std::invalid_argument("Trace length must be at least 1.");
    }

    const CountMatrix weighted = weighted_transition_matrix(system);
    const CountMatrix propagated = matrix_power(weighted, trace_length - 1);

    CountVector ones(system.states.size());
    ones.setOnes();

    return (system.valuation_counts.transpose() * propagated * ones)(0, 0);
}
