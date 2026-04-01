#pragma once

#include <cstddef>

#include "requirement.hpp"

/// Computes the syntactic similarity between two requirements. Currently not
/// implemented; exists as a placeholder for future work on syntactic metrics
/// for FRET requirements (e.g., comparing trigger/response formula structure).
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @return                  A syntactic similarity score (to be defined)
/// @throws std::logic_error This function is not yet implemented
double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement);
