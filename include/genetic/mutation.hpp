#pragma once

#include <string>
#include <vector>

#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"

/// Mutates a formula according to a propositional GA mutation strategy. Atom
/// names are replaced by atoms drawn from @p atoms; if @p atoms is empty,
/// atom names are left unchanged.
///
/// @param formula        The formula to mutate
/// @param atoms          Pool of atom names to draw replacements from
/// @param random_source  Random source for branch and selector choices
/// @return               A mutated formula
Formula mutate_formula(const Formula& formula,
                       const std::vector<std::string>& atoms,
                       const RandomSource& random_source);

/// Mutates a timing constraint using a timing-level mutation strategy.
///
/// @param timing        The timing value to mutate
/// @param random_source Random source for branch and selector choices
/// @return              A mutated timing value
Timing mutate_timing(const Timing& timing, const RandomSource& random_source);

/// Mutates a requirement. Atom names in the trigger and response are replaced
/// by atoms drawn from @p atoms; if @p atoms is empty, atom names are left
/// unchanged.
///
/// @param requirement   The requirement to mutate
/// @param atoms         Pool of atom names to draw replacements from
/// @param random_source Random source for branch and selector choices
/// @return              A mutated requirement
Requirement mutate_requirement(const Requirement& requirement,
                               const std::vector<std::string>& atoms,
                               const RandomSource& random_source);

/// Mutates a specification by picking one requirement at random and replacing
/// it with a mutated version. The pool of atom names is taken from the
/// specification's in_atoms and out_atoms.
///
/// @param specification The specification to mutate (must be non-empty)
/// @param random_source Random source for index and mutation choices
/// @return              A specification with one requirement mutated
/// @throws std::invalid_argument if specification is empty or random_source is
///         not callable
Specification mutate_specification(const Specification& specification,
                                   const RandomSource& random_source);
