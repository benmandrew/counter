#pragma once

// The two TLSF repair strategies. Both take the parsed original and return the
// repairs found, scored against it, for the driver to screen and write out.

#include <string>
#include <vector>

#include "config.hpp"
#include "evolve.hpp"
#include "fitness/function.hpp"
#include "genetic/pipeline.hpp"
#include "genetic/random_source.hpp"
#include "genetic/scored.hpp"
#include "tlsf/specification.hpp"

namespace tlsf::internal {

// Monolithic repair: evolve the whole spec once, collect realizable survivors.
// @p output_dir is where the accumulator streams each gate-passing candidate as
// it finds it, under cfg.accumulate_repairs; nothing is created there
// otherwise.
std::vector<Scored<Specification>> run_monolithic(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness,
    const DashboardProgress& progress, const std::string& output_dir,
    SearchBudget& budget);

// MUC repair: iteratively extract a minimal unrealizable core, evolve only that
// sub-specification, reintegrate the best realizable-on-sub-spec repair with
// the untouched non-core guarantees, and repeat on the recombined spec until it
// is realizable or the iteration cap trips. Returns the single realizable
// repair (scored against the original for output), or empty if none was found.
std::vector<Scored<Specification>> run_muc(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& output_fitness,
    const DashboardProgress& progress, SearchBudget& budget);

}  // namespace tlsf::internal
