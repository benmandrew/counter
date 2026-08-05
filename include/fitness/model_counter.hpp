#pragma once

/// @file model_counter.hpp
/// @brief Trace counting via transfer matrix exponentiation: counts the number
///        of valid bounded traces in a requirement automaton.

#include <cstddef>

#include "fitness/transfer_matrix.hpp"

/// Counts the traces of length exactly k accepted by a requirement automaton,
/// using transfer matrix exponentiation. Computes e_0^T * T^k * m, where T is
/// the weighted transition matrix, k is step_count, e_0 selects matrix index 0
/// -- the initial state, which build_transfer_system_from_hoa permutes there --
/// and m is @c TransferSystem::m_final_state_mask. The mask is the acceptance
/// vector of a Buchi automaton; safety automata leave it empty and an all-ones
/// vector is substituted, since every one of their states accepts.
///
/// This is the core model-counting operation used in semantic similarity
/// computation.
///
/// @param system The automaton (TransferSystem) to count traces in
/// @param step_count The trace length k
/// @return The number of accepted traces of length exactly k
Count count_traces(const TransferSystem& system, std::size_t step_count);
