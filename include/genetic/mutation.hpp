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
/// Always has no strengthening and Eventually no weakening: each is already
/// the extreme of the order in that direction. Always is additionally never
/// weakened, so it is a fixed point in both directions.
///
/// Eventually *is* strengthened, but only into a timing donated by
/// @p timing_pool, never one conjured from nothing. Each quantified donor
/// contributes `for n ticks` for its own tick count; a donated Immediately or
/// NextTimepoint contributes itself. With no usable donor Eventually is
/// returned unchanged. Timings are the same kind of resource as atom names in
/// mutate_formula: drawn from the specification, not invented.
///
/// @param timing        The timing value to mutate
/// @param direction     Whether to weaken or strengthen the timing
/// @param timing_pool   Timings occurring in the specification, donating the
///                      tick counts a strengthened Eventually may take
/// @param random_source Random source for branch and selector choices
/// @return              A mutated timing value
Timing mutate_timing(const Timing& timing, Direction direction,
                     const std::vector<Timing>& timing_pool,
                     const RandomSource& random_source);

/// Collects the distinct timings occurring in @p specification, for use as the
/// @p timing_pool argument of mutate_timing.
std::vector<Timing> collect_timing_pool(const Specification& specification);

/// Mutates a requirement. Each of trigger, response, and timing is mutated
/// independently with probabilities from @p cfg. Response atoms are drawn from
/// @p atoms; trigger (condition) atoms are drawn only from @p condition_atoms,
/// so that output atoms never leak into a trigger.
///
/// @param requirement     The requirement to mutate
/// @param atoms           Pool of atom names for response mutation
/// @param condition_atoms Pool of atom names for trigger mutation (inputs only)
/// @param direction       Direction applied to the timing field
/// @param timing_pool     Timings donating tick counts to timing mutation
/// @param random_source   Random source for branch and selector choices
/// @param cfg             Configuration providing mutation probabilities
/// @return                A mutated requirement
Requirement mutate_requirement(const Requirement& requirement,
                               const std::vector<std::string>& atoms,
                               const std::vector<std::string>& condition_atoms,
                               Direction direction,
                               const std::vector<Timing>& timing_pool,
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
