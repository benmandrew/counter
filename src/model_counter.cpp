#include "model_counter.hpp"

#include <stdexcept>
#include <string>

namespace {

Count checked_add_or_throw(Count lhs, Count rhs, const char* operation) {
    Count result = 0;
    if (count_add_overflow(lhs, rhs, result)) {
        throw std::overflow_error(std::string("Count overflow during ") +
                                  operation + ".");
    }
    return result;
}

Count checked_mul_or_throw(Count lhs, Count rhs, const char* operation) {
    Count result = 0;
    if (count_mul_overflow(lhs, rhs, result)) {
        throw std::overflow_error(std::string("Count overflow during ") +
                                  operation + ".");
    }
    return result;
}

CountMatrix checked_matrix_multiply(const CountMatrix& lhs,
                                    const CountMatrix& rhs) {
    if (lhs.cols() != rhs.rows()) {
        throw std::invalid_argument(
            "Incompatible matrix dimensions for multiplication.");
    }
    CountMatrix product(lhs.rows(), rhs.cols());
    product.setZero();
    for (Eigen::Index row = 0; row < lhs.rows(); ++row) {
        for (Eigen::Index inner = 0; inner < lhs.cols(); ++inner) {
            const Count left = lhs(row, inner);
            if (left == 0) {
                continue;
            }
            for (Eigen::Index column = 0; column < rhs.cols(); ++column) {
                const Count right = rhs(inner, column);
                if (right == 0) {
                    continue;
                }
                const Count term =
                    checked_mul_or_throw(left, right, "matrix multiplication");
                product(row, column) = checked_add_or_throw(
                    product(row, column), term, "matrix multiplication");
            }
        }
    }
    return product;
}

CountVector checked_matrix_vector_multiply(const CountMatrix& matrix,
                                           const CountVector& vector) {
    if (matrix.cols() != vector.rows()) {
        throw std::invalid_argument(
            "Incompatible dimensions for matrix-vector multiplication.");
    }
    CountVector result(matrix.rows());
    result.setZero();
    for (Eigen::Index row = 0; row < matrix.rows(); ++row) {
        Count sum = 0;
        for (Eigen::Index column = 0; column < matrix.cols(); ++column) {
            const Count lhs = matrix(row, column);
            const Count rhs = vector(column);
            if (lhs == 0 || rhs == 0) {
                continue;
            }
            const Count term =
                checked_mul_or_throw(lhs, rhs, "matrix-vector multiplication");
            sum =
                checked_add_or_throw(sum, term, "matrix-vector multiplication");
        }
        result(row) = sum;
    }
    return result;
}

Count checked_dot_product(const CountVector& lhs, const CountVector& rhs) {
    if (lhs.rows() != rhs.rows()) {
        throw std::invalid_argument("Incompatible vector dimensions.");
    }
    Count sum = 0;
    for (Eigen::Index index = 0; index < lhs.rows(); ++index) {
        const Count left = lhs(index);
        const Count right = rhs(index);
        if (left == 0 || right == 0) {
            continue;
        }
        const Count term = checked_mul_or_throw(left, right, "dot product");
        sum = checked_add_or_throw(sum, term, "dot product");
    }
    return sum;
}

CountMatrix matrix_power(const CountMatrix& matrix, std::size_t exponent) {
    if (matrix.rows() != matrix.cols()) {
        throw std::invalid_argument("Transfer matrices must be square.");
    }
    CountMatrix result = CountMatrix::Identity(matrix.rows(), matrix.cols());
    CountMatrix factor = matrix;
    while (exponent > 0) {
        if ((exponent & 1U) != 0U) {
            result = checked_matrix_multiply(result, factor);
        }
        exponent >>= 1U;
        if (exponent > 0) {
            factor = checked_matrix_multiply(factor, factor);
        }
    }
    return result;
}

}  // namespace

Count count_traces(const TransferSystem& system, std::size_t trace_length) {
    if (trace_length == 0) {
        throw std::invalid_argument("Trace length must be at least 1.");
    }
    const CountMatrix weighted = weighted_transition_matrix(system);
    const CountMatrix propagated = matrix_power(weighted, trace_length - 1);
    CountVector ones(system.m_states.size());
    ones.setOnes();
    const CountVector propagated_ones =
        checked_matrix_vector_multiply(propagated, ones);
    return checked_dot_product(system.m_valuation_counts, propagated_ones);
}
