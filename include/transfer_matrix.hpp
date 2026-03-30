#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "requirement.hpp"

#if defined(COUNTER_USE_UINT128)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
using Count = unsigned __int128;
#pragma GCC diagnostic pop
#else
using Count = std::uint64_t;
#endif
using CountMatrix = Eigen::Matrix<Count, Eigen::Dynamic, Eigen::Dynamic>;
using CountVector = Eigen::Matrix<Count, Eigen::Dynamic, 1>;

inline bool count_add_overflow(Count lhs, Count rhs, Count& result) {
    const Count max_value = std::numeric_limits<Count>::max();
    if (lhs > max_value - rhs) {
        return true;
    }
    result = lhs + rhs;
    return false;
}

inline bool count_mul_overflow(Count lhs, Count rhs, Count& result) {
    if (lhs == 0 || rhs == 0) {
        result = 0;
        return false;
    }
    const Count max_value = std::numeric_limits<Count>::max();
    if (lhs > max_value / rhs) {
        return true;
    }
    result = lhs * rhs;
    return false;
}

inline std::string count_to_string(Count value) {
#if defined(COUNTER_USE_UINT128)
    if (value == 0) {
        return "0";
    }
    std::string digits;
    while (value > 0) {
        const unsigned digit = static_cast<unsigned>(value % 10U);
        digits.push_back(static_cast<char>('0' + digit));
        value /= 10U;
    }
    std::reverse(digits.begin(), digits.end());
    return digits;
#else
    return std::to_string(value);
#endif
}

inline Count parse_count_decimal_or_throw(std::string_view text) {
    if (text.empty()) {
        throw std::invalid_argument("Count text must not be empty.");
    }
    Count value = 0;
    const Count max_value = std::numeric_limits<Count>::max();
    for (const char character : text) {
        if (!std::isdigit(static_cast<unsigned char>(character))) {
            throw std::invalid_argument("Count text must contain only digits.");
        }
        const Count digit = static_cast<Count>(character - '0');
        if (value > (max_value - digit) / 10U) {
            throw std::overflow_error("Parsed count does not fit Count type.");
        }
        value = value * 10U + digit;
    }
    return value;
}

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

CountVector count_joint_valuation_counts(const Requirement& requirement1,
                                         const Requirement& requirement2,
                                         unsigned seed = 1);

CountMatrix weighted_transition_matrix(const TransferSystem& system);

CountMatrix build_combined_weighted_transition_matrix(
    const Requirement& requirement1, const Requirement& requirement2,
    const CountVector& joint_valuation_counts = CountVector());
