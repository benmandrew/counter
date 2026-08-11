#pragma once

/// @file guarantee_parts.hpp
/// @brief Guarantee-side decomposition for the MRS status score: splitting the
///        system sections into conjuncts, and rebuilding a specification from a
///        subset of them.
///
/// Internal to the TLSF front end, so it lives under src/ rather than include/.

#include <cstddef>
#include <vector>

#include "tlsf/mucs.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

/// The guarantee-side formulae of `spec` (PRESET, then ASSERT, then GUARANTEE),
/// split into top-level conjuncts and each tagged with the section it came
/// from. Reuses CoreFormula and the section order the MUC extractor enumerates
/// in, so the two agree on what the guarantee side is made of.
///
/// Splitting is what makes the MRS score grade rather than plateau. On
/// `detector` the undecomposed score is flat at 0.5 across all six of its
/// unrealizable links, because the specification's two guarantees admit only
/// three values; split, the same chain runs 0.143 through 0.857 to 1.0. Across
/// `examples/` splitting takes the median number of grade levels from 3 to 6.
///
/// Two rewrites apply, both language-preserving, so the split subset walk asks
/// about the same specification the unsplit one would:
///   - `A & B` becomes `A`, `B`
///   - `G(A & B)` becomes `G A`, `G B`
/// Nothing else is descended into. A part keeps its section tag, and since the
/// lowering a section applies distributes over conjunction, reassembling every
/// part of a section reproduces that section's contribution exactly.
std::vector<CoreFormula> split_guarantee_parts(const Specification& spec);

/// `spec` with its environment side, atoms and metadata unchanged, and only the
/// parts at `indices` on the guarantee side. Indices address the vector
/// split_guarantee_parts returned for that same specification.
Specification build_part_subset(const Specification& spec,
                                const std::vector<CoreFormula>& parts,
                                const std::vector<std::size_t>& indices);

}  // namespace tlsf
