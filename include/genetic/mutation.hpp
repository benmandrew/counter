#pragma once

#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"

/// Mutates a formula according to a propositional GA mutation strategy.
///
/// @param formula        The formula to mutate
/// @param random_source  Random source for branch and selector choices
/// @return               A mutated formula
Formula mutate_formula(const Formula& formula,
                       const RandomSource& random_source);

/// Mutates a timing constraint using a timing-level mutation strategy.
///
/// @param timing        The timing value to mutate
/// @param random_source Random source for branch and selector choices
/// @return              A mutated timing value
Timing mutate_timing(const Timing& timing, const RandomSource& random_source);

/// Mutates a requirement with an explicit random source.
///
/// @param requirement   The requirement to mutate
/// @param random_source Random source for branch and selector choices
/// @return              A mutated requirement
Requirement mutate_requirement(const Requirement& requirement,
                               const RandomSource& random_source);
