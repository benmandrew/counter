#pragma once

#include <utility>

#include "genetic/random_source.hpp"
#include "requirement.hpp"

/// Produces an offspring requirement by crossing over the parents' trigger,
/// response, and timing components.
///
/// @param first_parent  First parent requirement
/// @param second_parent Second parent requirement
/// @param random_bool   Random boolean source used to select crossover paths
/// @return              Offspring requirement
Requirement crossover_requirements(const Requirement& first_parent,
                                   const Requirement& second_parent,
                                   const RandomSource& random_bool);
