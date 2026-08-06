#pragma once

#include <cstddef>
#include <string>
#include <vector>

// counter's end-of-run reports, printed to stdout after the search finishes.

// One filter's input and output population sizes, summed over every generation
// it ran in.
struct FilterRunStats {
    std::string name;
    std::size_t total_in{0};
    std::size_t total_out{0};
};

void print_filter_report(const std::vector<FilterRunStats>& stats);

void print_scoring_report();

void print_timing_report();

void print_cpu_report(double wall_s);
