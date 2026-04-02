#pragma once

#include "requirement.hpp"

/// Mutates a requirement according to a genetic algorithm strategy.
///
/// @param requirement The requirement to mutate
/// @return            A mutated requirement
/// @throws std::logic_error This function is not yet implemented
Requirement mutate_requirement(const Requirement& requirement);
