#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "fitness/transfer_matrix.hpp"

// The widest formula this counter accepts, in distinct atoms. Above it the
// enumeration is declined and the caller falls back to the model counter.
//
// The guards it exists for are narrow. Over the 2,932 distinct guard labels
// nine specifications produce, the widest mentions 12 atoms and 38.7% mention
// five or fewer, so 16 clears the measured distribution with room over it
// while bounding the work at 65,536 assignments -- 1,024 machine words per
// node of the formula.
inline constexpr std::size_t k_exhaustive_count_max_atoms = 16;

/// Counts the satisfying assignments of @p formula over the atoms it mentions,
/// by enumerating every one of them 64 assignments at a time.
///
/// Returns no answer where the enumeration does not apply: a string this
/// process cannot parse, a formula carrying a temporal operator (an assignment
/// to the atoms does not decide one, a trace does), or one wider than
/// k_exhaustive_count_max_atoms.
[[nodiscard]] std::optional<Count> count_models_exhaustively(
    const std::string& formula);
