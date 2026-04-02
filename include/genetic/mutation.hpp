#pragma once

#include <functional>

#include "prop_formula.hpp"
#include "requirement.hpp"

using BooleanRandomSource = std::function<bool()>;

/// Mutates a formula according to a propositional GA mutation strategy.
///
/// @param formula               The formula to mutate
/// @param boolean_random_source A source of random booleans used to choose
///                              mutation branches
/// @return                      A mutated formula
Formula mutate_formula(const Formula& formula,
                       const BooleanRandomSource& boolean_random_source);

/// Mutates a requirement according to a genetic algorithm strategy.
///
/// @param requirement The requirement to mutate
/// @return            A mutated requirement with an unchanged trigger/timing
Requirement mutate_requirement(const Requirement& requirement);

/// Mutates a requirement with an explicit random boolean source.
///
/// @param requirement           The requirement to mutate
/// @param boolean_random_source A source of random booleans used to choose
///                              mutation branches
/// @return                      A mutated requirement with an unchanged
///                              trigger/timing
Requirement mutate_requirement(
    const Requirement& requirement,
    const BooleanRandomSource& boolean_random_source);
