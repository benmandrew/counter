#pragma once

#include <cstddef>

#include "transfer_matrix.hpp"

/// Counts the number of valid traces of length step_count in a requirement
/// automaton using transfer matrix exponentiation. Computes: e^T * T^k * 1
/// where e is the indicator vector for the initial state, T is the transition
/// matrix, k is step_count, and 1 is the all-ones vector.
///
/// This is the core model-counting operation used in semantic similarity
/// computation: counting how many traces of bounded length satisfy a
/// requirement.
///
/// @param system The automaton (TransferSystem) to count traces in
/// @param step_count The maximum trace length k to count
/// @return The number of valid traces of length ≤ step_count
Count count_traces(const TransferSystem& system, std::size_t step_count);
