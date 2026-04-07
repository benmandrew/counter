#pragma once

#include <cstddef>

#include "requirement.hpp"

/// Computes syntactic similarity between two requirements by comparing the
/// trigger and response formulas, then averaging those scores with a timing
/// component.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @return                  A syntactic similarity score in the range [0, 1]
double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement);
