#pragma once

#include <utility>

#include "requirement.hpp"

/// Produces offspring requirements by crossing over two parent requirements.
///
/// @param first_parent  First parent requirement
/// @param second_parent Second parent requirement
/// @return              Offspring requirement
/// @throws std::logic_error This function is not yet implemented
Requirement crossover_requirements(const Requirement& first_parent,
                                   const Requirement& second_parent);
