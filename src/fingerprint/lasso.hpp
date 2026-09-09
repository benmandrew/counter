#pragma once

/// @file lasso.hpp
/// @brief LTL evaluation over ultimately periodic words, for behavioural
///        fingerprinting of repairs.

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "prop_formula.hpp"

namespace fingerprint {

/// The largest lasso a word may span. Every position of a word is one bit of
/// a `std::uint64_t` valuation mask, so the two together must fit in 64.
constexpr std::size_t k_max_positions = 64;

/// An ultimately periodic word: `m_n_positions` states, the last of which
/// loops back to `m_loop_start`. Each signal carries the mask of positions it
/// holds at rather than each position carrying a set of signals, because
/// evaluation walks the syntax tree once and folds whole masks, where the
/// row-wise shape would walk it once per position.
struct LassoWord {
    std::size_t m_loop_start = 0;
    std::size_t m_n_positions = 1;
    std::unordered_map<std::string, std::uint64_t> m_signal_masks;
};

/// The words one family is fingerprinted on.
///
/// Derived from @p signals, @p n_words and @p seed alone, and from the
/// signals in sorted order, so two tools' candidates for one family are
/// scored on the same words without a word file travelling between the hosts
/// that scored them. A fingerprint is only comparable against another drawn
/// from an identical call.
std::vector<LassoWord> sample_words(const std::vector<std::string>& signals,
                                    std::size_t n_words, std::uint64_t seed,
                                    std::size_t max_prefix,
                                    std::size_t max_cycle);

/// One bit per word, set where @p formula holds at position 0 of that word.
///
/// An atom no word names is false at every position. That covers the `false`
/// constant and a signal the specification declares without a word carrying
/// it; `true` is recognised by name, as it is everywhere else in the codebase
/// (is_constant_atom in src/requirement.cpp).
///
/// @throws std::invalid_argument if @p formula does not render to a string
///         this codebase's own parser reads back.
std::vector<bool> fingerprint_of(const Formula& formula,
                                 const std::vector<LassoWord>& words);

/// Lowercase hex, four bits a digit, word 0 in the low bit of the last digit.
/// Padded to a whole digit so a fingerprint's length names its word count.
std::string to_hex(const std::vector<bool>& bits);

/// The count of positions where two equal-length fingerprints differ.
std::size_t hamming_distance(const std::vector<bool>& lhs,
                             const std::vector<bool>& rhs);

}  // namespace fingerprint
