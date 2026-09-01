#pragma once

/// @file syntactic_similarity.hpp
/// @brief Syntactic similarity between requirements and specifications by
///        comparing shared sub-formula structure.

#include <cstddef>

#include "config.hpp"
#include "requirement.hpp"

/// Computes syntactic similarity between two requirements as the equally
/// weighted mean of five components: the formula-level similarity of the two
/// triggers, that of the two responses, and one each for the timing, the scope
/// and the condition type.
///
/// The timing, scope and condition-type components are all Jaccard overlaps of
/// downward closures in the field's implication order, so none of them carries
/// a chosen penalty. The condition-type order has two elements — Continual
/// implies Trigger — which fixes its two values at 1 and 1/2.
///
/// The scope component is the exception, being the Jaccard overlap of the
/// timepoint regions the two scopes enforce over, averaged with whether they
/// name the same mode. It is deliberately not the implication order the
/// `p_scope` mutation arm walks: that order depends on the timing as well as
/// the scope, and a similarity between two scopes has to be a property of the
/// scopes alone.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param cfg               Unused; kept so every fitness component shares one
///                          signature, as tlsf_syntactic_similarity does
/// @return                  A syntactic similarity score in the range [0, 1]
double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement,
                            const Config& cfg);

/// Computes syntactic similarity between two specifications from five
/// components: the formula-level similarity of the two trigger conjunctions,
/// that of the two response conjunctions, and the per-index averages of the
/// requirements' timing, scope and condition-type similarities. Each
/// specification's triggers are conjoined into one formula and its responses
/// into another. The five components are combined as an equally-weighted mean.
///
/// The last three components pair requirements by index rather than conjoining
/// them, because slot i of a candidate descends from slot i of the original and
/// that is the only pairing which compares a requirement against what it came
/// from. Each divides by the larger of the two requirement counts, so a
/// candidate that gained an assumption or tombstoned a guarantee scores below 1
/// on all three even where every matched pair agrees. Adding the scope and
/// condition-type components therefore weighted that size-mismatch signal more
/// heavily, and it moved FRETISH search results: 12 of 15 seeded runs over the
/// three FRETISH examples returned different repairs, two of them one repair
/// more.
///
/// Both specifications must have at least one requirement; this is asserted,
/// so it goes unchecked under NDEBUG.
///
/// @param specification       The first specification to compare (non-empty)
/// @param other_specification The second specification to compare (non-empty)
/// @param cfg                 Unused; kept so every fitness component shares
///                            one signature
/// @return                    A syntactic similarity score in the range [0, 1]
double syntactic_similarity(const Specification& specification,
                            const Specification& other_specification,
                            const Config& cfg);
