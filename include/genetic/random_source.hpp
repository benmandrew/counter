#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>

// Returns a pseudo-random value for the requested range [0, upper_bound).
using RandomSource = std::function<std::size_t(std::size_t upper_bound)>;

inline std::size_t next_index(const RandomSource& random_source,
                              std::size_t upper_bound) {
    if (!random_source) {
        throw std::invalid_argument("random source must be callable.");
    }
    if (upper_bound == 0) {
        throw std::invalid_argument("upper_bound must be positive.");
    }
    // Normalize values into the requested range.
    return random_source(upper_bound) % upper_bound;
}

inline bool next_bool(const RandomSource& random_source) {
    return next_index(random_source, 2) == 1;
}
