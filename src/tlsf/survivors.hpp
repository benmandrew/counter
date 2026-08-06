#pragma once

// Screens over a scored population: which candidates count as repairs, and the
// two final passes that reduce the repairs to the ones worth writing out.

#include <vector>

#include "config.hpp"
#include "fitness/function.hpp"
#include "genetic/scored.hpp"
#include "runner/black.hpp"
#include "tlsf/specification.hpp"

namespace tlsf::internal {

// Realizable survivors of the population, deduplicated by value while
// preserving fitness order.
std::vector<Scored<Specification>> realizable_survivors(
    const std::vector<Scored<Specification>>& population, const Config& cfg,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness);

// Keeps the survivors the original logically implies. The check is exact rather
// than the FRETISH assume-guarantee decomposition -- a tlsf::Specification
// lowers to one LTL formula -- so a rejection means the candidate genuinely
// forbids behaviour the original allowed, not that the decomposition lost it.
std::vector<Scored<Specification>> keep_weakenings(
    const std::vector<Scored<Specification>>& survivors,
    const Specification& original, SatisfiabilityChecker& checker);

// Keeps the survivors not strictly implied by another, mirroring the FRETISH
// final implication filter.
std::vector<Scored<Specification>> keep_maximal(
    const std::vector<Scored<Specification>>& survivors,
    SatisfiabilityChecker& checker);

}  // namespace tlsf::internal
