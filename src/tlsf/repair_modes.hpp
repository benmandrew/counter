#pragma once

// The two TLSF repair strategies. Both take the parsed original and return the
// repairs found, scored against it, for the driver to screen and write out.

#include <vector>

#include "config.hpp"
#include "evolve.hpp"
#include "fitness/function.hpp"
#include "genetic/random_source.hpp"
#include "genetic/scored.hpp"
#include "tlsf/specification.hpp"

namespace tlsf::internal {

// Monolithic repair: evolve the whole spec once, collect realizable survivors.
std::vector<Scored<Specification>> run_monolithic(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness,
    const DashboardProgress& progress);

// MUC repair: iteratively extract a minimal unrealizable core, evolve only that
// sub-specification, reintegrate the best realizable-on-sub-spec repair with
// the untouched non-core guarantees, and repeat on the recombined spec until it
// is realizable or the iteration cap trips. Returns the single realizable
// repair (scored against the original for output), or empty if none was found.
std::vector<Scored<Specification>> run_muc(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& output_fitness,
    const DashboardProgress& progress);

}  // namespace tlsf::internal
