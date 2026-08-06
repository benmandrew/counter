#pragma once

/// @file syntactic_similarity.hpp
/// @brief Syntactic similarity between requirements and specifications by
///        comparing shared sub-formula structure.

#include <cstddef>

#include "config.hpp"
#include "requirement.hpp"

/// Computes syntactic similarity between two requirements by comparing the
/// trigger and response formulas, then averaging those scores with a timing
/// component.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param cfg               Unused; kept so every fitness component shares one
///                          signature, as tlsf_syntactic_similarity does
/// @return                  A syntactic similarity score in the range [0, 1]
double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement,
                            const Config& cfg);

/// Computes syntactic similarity between two specifications from three
/// components: the formula-level similarity of the two trigger conjunctions,
/// that of the two response conjunctions, and the per-index average of the
/// requirements' timing similarities. Each specification's triggers are
/// conjoined into one formula and its responses into another. The three
/// components are combined as an equally-weighted mean.
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
