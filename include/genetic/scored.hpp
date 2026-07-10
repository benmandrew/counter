#pragma once

/// @file scored.hpp
/// @brief The Scored<Spec> template pairing a specification element with its
///        aggregated fitness score.

/// A specification element paired with its aggregated fitness score.
template <typename Spec>
struct Scored {
    Spec specification;
    double fitness = 0.0;
};
