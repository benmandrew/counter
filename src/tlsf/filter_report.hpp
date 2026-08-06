#pragma once

// Per-filter population counts gathered over an evolve run, and the
// end-of-run report printed from them.

#include <cstddef>
#include <string>
#include <vector>

namespace tlsf::internal {

struct FilterRunStats {
    std::string name;
    std::size_t total_in = 0;
    std::size_t total_out = 0;
};

// Merges one evolve run's per-filter totals into a running aggregate (the MUC
// loop sums them across iterations for a single end-of-run report). Filters are
// built in the same order every call, so accumulation is positional.
void accumulate_filter_stats(std::vector<FilterRunStats>& aggregate,
                             const std::vector<FilterRunStats>& run);

void print_filter_report(const std::vector<FilterRunStats>& stats);

}  // namespace tlsf::internal
