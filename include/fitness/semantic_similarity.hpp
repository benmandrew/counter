#pragma once

/// @file semantic_similarity.hpp
/// @brief Semantic similarity between requirements and specifications using
///        bounded model counting of satisfying traces.

#include <cstddef>

#include "config.hpp"
#include "fitness/transfer_matrix.hpp"
#include "requirement.hpp"

/// Computes the semantic similarity between two requirements using bounded
/// model counting of satisfying traces. The similarity metric is defined as:
///   0.5 * ((shared(req, other, k) / count(req, k)) + (shared(req, other, k) /
///   count(other, k)))
/// where shared(req, other, k) is the number of traces of length k satisfying
/// both requirements, and count(req, k) is the number of traces satisfying req.
/// Returns a value between 0 and 1, where 1 indicates identical trace
/// semantics.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param step_count        The bound k on trace length for model counting
/// @return                  A semantic similarity score in [0, 1]
/// @throws std::domain_error if either requirement has zero satisfying traces
double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count);

/// Overload of semantic_similarity using the bound from @p cfg.
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param cfg               Configuration providing the model-counting bound
/// @return                  A semantic similarity score in [0, 1]
double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           const Config& cfg);

/// Computes semantic similarity between two specifications by averaging the
/// pairwise semantic similarities of corresponding requirements (matched in
/// set order). Both specifications must be non-empty and have the same number
/// of requirements.
///
/// @param specification       The first specification to compare (non-empty)
/// @param other_specification The second specification to compare (non-empty)
/// @param step_count          The bound k on trace length for model counting
/// @return                    A semantic similarity score in [0, 1]
double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           std::size_t step_count);

/// Overload of specification-level semantic_similarity using the bound from
/// @p cfg.
/// @param specification       The first specification to compare (non-empty)
/// @param other_specification The second specification to compare (non-empty)
/// @param cfg                 Configuration providing the model-counting bound
/// @return                    A semantic similarity score in [0, 1]
double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           const Config& cfg);
