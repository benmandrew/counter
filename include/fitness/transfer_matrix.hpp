#pragma once

#include <Eigen/Dense>
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <limits>
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

/// Alias for a dense matrix of Count values, used to represent transition
/// matrices in automata for model counting.
using CountMatrix = Eigen::Matrix<Count, Eigen::Dynamic, Eigen::Dynamic>;

/// Alias for a dense vector of Count values, used to represent valuation
/// counts and trace counts in model counting computations.
using CountVector = Eigen::Matrix<Count, Eigen::Dynamic, 1>;

/// Checks for overflow when adding two Count values and stores the result
/// if no overflow occurs.
/// @param lhs Left-hand side operand
/// @param rhs Right-hand side operand
/// @param result Output parameter to store the sum if no overflow
/// @return true if overflow would occur, false otherwise
inline bool count_add_overflow(Count lhs, Count rhs, Count& result) {
    const Count max_value = std::numeric_limits<Count>::max();
    if (lhs > max_value - rhs) {
        return true;
    }
    result = lhs + rhs;
    return false;
}

/// Checks for overflow when multiplying two Count values and stores the result
/// if no overflow occurs.
/// @param lhs Left-hand side operand
/// @param rhs Right-hand side operand
/// @param result Output parameter to store the product if no overflow
/// @return true if overflow would occur, false otherwise
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

/// Converts a Count value to a decimal string representation. Handles both
/// 64-bit and 128-bit Count types transparently.
/// @param value The Count value to convert
/// @return A string representation in decimal
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

/// Parses a count value from a decimal string representation.
/// @param text A string of decimal digits
/// @return The parsed Count value
/// @throws std::invalid_argument if the text contains non-digits
/// @throws std::overflow_error if the parsed value exceeds Count::max
inline Count parse_count_decimal_or_throw(std::string_view text) {
    assert(!text.empty());
    Count value = 0;
    const Count max_value = std::numeric_limits<Count>::max();
    for (const char character : text) {
        assert(std::isdigit(static_cast<unsigned char>(character)));
        const Count digit = static_cast<Count>(character - '0');
        assert(value <= (max_value - digit) / 10U);
        value = value * 10U + digit;
    }
    return value;
}

/// An automaton representation for model counting. Encodes a finite-state
/// automaton that validates a requirement specification, along with valuation
/// counts for the propositional conditions and a transition matrix for
/// computing the number of satisfying traces via matrix exponentiation.
struct TransferSystem {
    /// The states of the automaton
    std::vector<State> m_states;
    /// Counts of satisfying valuations for conditions
    CountVector m_valuation_counts;
    /// Transition matrix T where T[i][j] is count from state i to j
    CountMatrix m_transition_matrix;
    /// Whether matrix represents weighted transitions
    bool m_transition_matrix_is_weighted = false;
};

/// Constructs a TransferSystem automaton from a requirement. The automaton
/// structure varies based on the requirement's Timing constraint and encodes
/// the valid traces that satisfy the requirement specification.
///
/// @param requirement The requirement to build an automaton for
/// @param canonical_valuation_counts Optional pre-computed valuation counts
/// @return A TransferSystem representing the requirement automaton
TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts = CountVector());

/// Counts the number of satisfying valuations for the canonical boolean
/// conditions {T,R} ∈ {true, false} of a requirement using a propositional
/// model counter (e.g., Ganak for bounded model counting).
///
/// @param requirement The requirement whose conditions to count
/// @param seed Random seed for the model counter
/// @return A vector of four counts: [¬T∧¬R, ¬T∧R, T∧¬R, T∧R]
CountVector count_canonical_valuation_counts(const Requirement& requirement,
                                             unsigned seed = 1);

/// Counts the joint satisfying valuations for the cross-product of conditions
/// from two requirements, producing 16 counts for all combinations of
/// {T₁,R₁,T₂,R₂} ∈ {true, false}. Used for computing model counts of
/// requirement conjunctions in semantic similarity.
///
/// @param requirement1 The first requirement
/// @param requirement2 The second requirement
/// @param seed Random seed for the model counter
/// @return A vector of 16 counts for all valuation combinations
CountVector count_joint_valuation_counts(const Requirement& requirement1,
                                         const Requirement& requirement2,
                                         unsigned seed = 1);

CountMatrix weighted_transition_matrix(const TransferSystem& system);

CountMatrix build_combined_weighted_transition_matrix(
    const Requirement& requirement1, const Requirement& requirement2,
    const CountVector& joint_valuation_counts = CountVector());
