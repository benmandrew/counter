#pragma once

/// @file semantic_similarity.hpp
/// @brief Semantic similarity between requirements and specifications using
///        bounded model counting of satisfying traces.

#include <cstddef>

#include "config.hpp"
#include "fitness/transfer_matrix.hpp"
#include "requirement.hpp"

/// The three trace counts a requirement pair yields over a shared atom
/// universe and bound. @c m_conjunction_count counts traces satisfying both
/// requirements; mathematically it is at most either individual count, but
/// only up to floating-point rounding now that @c Count is a float.
struct SemanticSimilarityCounts {
    Count m_requirement_count;
    Count m_other_requirement_count;
    Count m_conjunction_count;
};

/// Combines the three trace counts into a similarity score in [0, 1]. Both
/// metrics take the harmonic mean of two directional containment terms and
/// differ only in that term: @c SimilarityMetric::Direct uses the count ratio
/// (a Sorensen-Dice overlap, clamped to [0, 1] since rounding can push
/// @c m_conjunction_count a few ulps past an individual count now that
/// @c Count is a float), @c SimilarityMetric::Logarithmic the ratio of the
/// counts' logarithms (a bound-stable growth-rate comparison). Exposed for
/// testing; the @c Requirement overloads are the normal entry points.
double semantic_similarity_from_counts(
    const SemanticSimilarityCounts& counts,
    SimilarityMetric metric = SimilarityMetric::Direct);

/// Computes the semantic similarity between two requirements using bounded
/// model counting of satisfying traces. With @c SimilarityMetric::Direct the
/// score is the harmonic mean of the two directional count ratios:
///   2 * shared / (count(req) + count(other))   [over length-k traces]
/// where shared(req, other, k) is the number of traces of length k satisfying
/// both requirements, and count(req, k) is the number of traces satisfying req.
/// Returns a value between 0 and 1, where 1 indicates identical trace
/// semantics.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param step_count        The bound k on trace length for model counting
/// @param metric            Whether to combine counts directly or via logarithm
/// @return                  A semantic similarity score in [0, 1]
/// @throws std::domain_error if either requirement has zero satisfying traces
double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count,
                           SimilarityMetric metric = SimilarityMetric::Direct);

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
/// @param metric              Whether to combine counts directly or via log
/// @return                    A semantic similarity score in [0, 1]
double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           std::size_t step_count,
                           SimilarityMetric metric = SimilarityMetric::Direct);

/// Overload of specification-level semantic_similarity using the bound from
/// @p cfg.
/// @param specification       The first specification to compare (non-empty)
/// @param other_specification The second specification to compare (non-empty)
/// @param cfg                 Configuration providing the model-counting bound
/// @return                    A semantic similarity score in [0, 1]
double semantic_similarity(const Specification& specification,
                           const Specification& other_specification,
                           const Config& cfg);
