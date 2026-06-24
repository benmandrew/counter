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
/// @param cfg               Configuration providing component weights
/// @return                  A syntactic similarity score in the range [0, 1]
double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement,
                            const Config& cfg);

/// Computes syntactic similarity between two specifications by conjoining all
/// triggers into a single formula and all responses into a single formula for
/// each specification, then averaging the formula-level similarities of the
/// two trigger conjunctions and the two response conjunctions.
///
/// @param specification       The first specification to compare (non-empty)
/// @param other_specification The second specification to compare (non-empty)
/// @param cfg                 Configuration providing component weights
/// @return                    A syntactic similarity score in the range [0, 1]
/// @throws std::invalid_argument if either specification has no requirements
double syntactic_similarity(const Specification& specification,
                            const Specification& other_specification,
                            const Config& cfg);
