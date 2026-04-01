#pragma once

#include <cstddef>

#include "requirement.hpp"
#include "transfer_matrix.hpp"

/// Computes the semantic similarity between two requirements using bounded
/// model counting of satisfying traces. The similarity metric is defined as:
///   (shared(req, other, k) / count(req, k)) +
///   (shared(req, other, k) / count(other, k))
/// where shared(req, other, k) is the number of traces of length k satisfying
/// both requirements, and count(req, k) is the number of traces satisfying req.
/// Returns a value between 0 and 2, where 2 indicates identical trace
/// semantics.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param step_count        The bound k on trace length for model counting
/// @return                  A semantic similarity score in [0, 2]
/// @throws std::domain_error if either requirement has zero satisfying traces
double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count);

/// Overload of semantic_similarity using a default model-counting bound.
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @return                  A semantic similarity score in [0, 2]
double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement);
