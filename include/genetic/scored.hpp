#pragma once

/// @file scored.hpp
/// @brief The Scored<Spec> template pairing a specification element with its
///        aggregated fitness score and multi-objective selection metadata.

#include <cstddef>
#include <vector>

/// A specification element paired with its fitness scores.
///
/// @c fitness is the weighted-average scalar used by the default selection
/// scheme, by diagnostics, and by the output records. @c objectives holds the
/// individual per-objective scores (registration order) that NSGA-II selection
/// ranks over; @c rank and @c crowding_distance are the NSGA-II attributes
/// derived from those objectives (unused, left at their defaults, under the
/// weighted-average scheme).
template <typename Spec>
struct Scored {
    Spec specification;
    double fitness = 0.0;
    std::vector<double> objectives;
    std::size_t rank = 0;
    double crowding_distance = 0.0;
};
