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
        const auto digit = static_cast<unsigned>(value % 10U);
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
    [[maybe_unused]] const Count max_value = std::numeric_limits<Count>::max();
    for (const char character : text) {
        assert(std::isdigit(static_cast<unsigned char>(character)));
        const auto digit = static_cast<Count>(character - '0');
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
    /// Optional mask of valid final states (1 = valid, 0 = invalid).
    /// Empty means all states are valid final states.
    CountVector m_final_state_mask;
};

/// Returns the number of unique atoms across all four formulas of two
/// requirements. Used to establish a shared atom universe for semantic
/// similarity computations.
std::size_t count_joint_atoms(const Requirement& req1, const Requirement& req2);

/// Constructs a TransferSystem for a requirement using SPOT's ltl2tgba to
/// generate a deterministic automaton from the requirement's LTL formula.
/// The canonical_valuation_counts parameter is accepted for API compatibility
/// but is ignored (weights come from SPOT + Ganak directly).
TransferSystem build_transfer_system(
    const Requirement& requirement,
    const CountVector& canonical_valuation_counts = CountVector());

/// Constructs a TransferSystem for a requirement, explicitly specifying the
/// total atom universe size used for transition weighting. Use this overload
/// when comparing trace counts across systems that must share the same universe
/// (e.g. when computing semantic similarity alongside a conjunction system).
TransferSystem build_transfer_system(const Requirement& requirement,
                                     std::size_t n_total_atoms);

/// Constructs a TransferSystem for the conjunction of two requirements.
/// Runs ltl2tgba on "(ltl1) & (ltl2)" and weights transitions via Ganak.
TransferSystem build_conjunction_transfer_system(
    const Requirement& requirement1, const Requirement& requirement2);

/// Returns the weighted transition matrix for a TransferSystem. If
/// m_transition_matrix_is_weighted is already true the matrix is returned
/// unchanged. Otherwise each column j is scaled by m_valuation_counts(j),
/// turning a 0/1 adjacency matrix into a valuation-weighted one.
CountMatrix weighted_transition_matrix(const TransferSystem& system);
