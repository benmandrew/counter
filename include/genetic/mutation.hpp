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

/// Mutates a specification by picking one requirement at random and replacing
/// it with a mutated version.
///
/// @param specification The specification to mutate (must be non-empty)
/// @param random_source Random source for index and mutation choices
/// @return              A specification with one requirement mutated
/// @throws std::invalid_argument if specification is empty or random_source is
///         not callable
Specification mutate_specification(const Specification& specification,
                                   const RandomSource& random_source);
