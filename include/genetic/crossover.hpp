#pragma once

/// @file crossover.hpp
/// @brief Crossover operators that combine two parent requirements or
///        specifications to produce an offspring.

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "requirement.hpp"

/// Produces an offspring requirement by crossing over the parents' condition,
/// response, and timing components. Each of the two formula fields is grafted
/// as in AuRUS: with equal probability a subformula of the first parent's field
/// is replaced by one drawn from the second parent's, or the two are joined
/// under a fresh binary operator. Neither branch copies a field verbatim.
///
/// `cfg.repaired_operators` selects where within a field the graft lands: on,
/// the site is drawn uniformly over the field's nodes; off, a fair coin is
/// tossed at each node of a post-order walk, which can reach the end of the
/// walk having grafted nowhere and return the first parent's field unchanged.
///
/// @param first_parent  First parent requirement
/// @param second_parent Second parent requirement
/// @param random_source Random source for branch and selector choices
/// @param cfg           Configuration selecting the graft-site draw
/// @return              Offspring requirement
Requirement crossover_requirements(const Requirement& first_parent,
                                   const Requirement& second_parent,
                                   const RandomSource& random_source,
                                   const Config& cfg);

/// Produces an offspring specification by merging one requirement per side.
/// For the assumptions and again for the guarantees, one slot of @p
/// first_parent and one of @p second_parent are drawn uniformly and
/// independently — the donor need not, and usually does not, occupy the target
/// slot — and their crossover replaces the target slot. Every other slot is
/// inherited from @p first_parent, whose shape the offspring therefore keeps.
/// Deleted and non-weakenable requirements take no part on either side.
///
/// @param first_parent  First parent specification
/// @param second_parent Second parent specification
/// @param random_source Random source for slot and branch choices
/// @param cfg           Configuration selecting the graft-site draw
/// @return              Offspring specification, or @p first_parent unchanged
///                      if the parents' in/out atoms differ or either side has
///                      no eligible slot
Specification crossover_specifications(const Specification& first_parent,
                                       const Specification& second_parent,
                                       const RandomSource& random_source,
                                       const Config& cfg);
