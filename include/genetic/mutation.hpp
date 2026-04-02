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

/// Mutates a timing constraint using a timing-level mutation strategy.
///
/// @param timing                The timing value to mutate
/// @param boolean_random_source A source of random booleans used to choose
///                              mutation branches
/// @return                      A mutated timing value
Timing mutate_timing(const Timing& timing,
                     const BooleanRandomSource& boolean_random_source);

/// Mutates a requirement with an explicit random boolean source.
///
/// @param requirement           The requirement to mutate
/// @param boolean_random_source A source of random booleans used to choose
///                              mutation branches
/// @return                      A mutated requirement
Requirement mutate_requirement(
    const Requirement& requirement,
    const BooleanRandomSource& boolean_random_source);
