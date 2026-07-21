#pragma once

/// @file mutation.hpp
/// @brief Mutation operators for formulae, timing constraints, requirements,
///        and full specifications used in the genetic algorithm.

#include <cstdint>
#include <string>
#include <vector>

#include "config.hpp"
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

/// Direction of a mutation within the logical partial order on requirements:
/// Weaken admits more behaviours, Strengthen admits fewer.
enum class Direction : std::uint8_t { Weaken, Strengthen };

/// Mutates a timing constraint by taking a single step through the timing
/// partial order in the direction @p direction.
///
/// Always and Eventually are fixed points in *both* directions. Always has no
/// strengthening and Eventually no weakening, since each is already the
/// extreme of the order; neither is moved in the other direction either,
/// because stepping off an unquantified timing into the quantified families
/// would invent a tick count with no basis in the original requirement.
///
/// @param timing        The timing value to mutate
/// @param direction     Whether to weaken or strengthen the timing
/// @param random_source Random source for branch and selector choices
/// @return              A mutated timing value
Timing mutate_timing(const Timing& timing, Direction direction,
                     const RandomSource& random_source);

/// Mutates a requirement. Each of trigger, response, and timing is mutated
/// independently with probabilities from @p cfg. Response atoms are drawn from
/// @p atoms; trigger (condition) atoms are drawn only from @p condition_atoms,
/// so that output atoms never leak into a trigger.
///
/// @param requirement     The requirement to mutate
/// @param atoms           Pool of atom names for response mutation
/// @param condition_atoms Pool of atom names for trigger mutation (inputs only)
/// @param direction       Direction applied to the timing field
/// @param random_source   Random source for branch and selector choices
/// @param cfg             Configuration providing mutation probabilities
/// @return                A mutated requirement
Requirement mutate_requirement(const Requirement& requirement,
                               const std::vector<std::string>& atoms,
                               const std::vector<std::string>& condition_atoms,
                               Direction direction,
                               const RandomSource& random_source,
                               const Config& cfg);

/// Mutates a specification by picking one requirement at random and replacing
/// it with a mutated version. The pool of atom names is taken from the
/// specification's in_atoms and out_atoms.
///
/// @param specification The specification to mutate (must be non-empty)
/// @param random_source Random source for index and mutation choices
/// @param cfg           Configuration providing mutation probabilities
/// @return              A specification with one requirement mutated
/// @throws std::invalid_argument if specification is empty or random_source is
///         not callable
Specification mutate_specification(const Specification& specification,
                                   const RandomSource& random_source,
                                   const Config& cfg);
