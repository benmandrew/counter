#pragma once

/// @file semantic_similarity.hpp
/// @brief Semantic similarity between requirements and specifications using
///        bounded model counting of satisfying traces.

#include <cstddef>
#include <string>

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

/// Counts the length-@p step_count traces of @p ltl over a universe of
/// @p n_total_atoms atoms, memoised on all three arguments.
///
/// The transfer-matrix construction and exponentiation inside count_traces are
/// redone from scratch on every call, even though one side of a comparison is
/// frequently the same formula across an entire population — the original
/// specification's piece, compared against many mutated offspring. Both the
/// FRETISH and TLSF fitness paths count through here so they share one cache.
///
/// Thread-safe: the cache is guarded by an internal mutex.
Count cached_count_traces(const std::string& ltl, std::size_t n_total_atoms,
                          std::size_t step_count);

/// The largest trace length representable for @p n_atoms atoms.
///
/// count_traces sums at most 2^(n_atoms * k) traces, so k is only representable
/// while n_atoms * k stays inside Count's exponent range. Past it the products
/// inside count_traces saturate to infinity, and the assert guarding them is
/// compiled out under NDEBUG — so a release build yields silently wrong counts
/// rather than aborting. Callers must clamp their bound to this.
std::size_t max_representable_step_count(std::size_t n_atoms);

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
/// semantics. Zero counts are boundary cases rather than errors: two
/// unsatisfiable requirements score 1.0, and one unsatisfiable against one
/// satisfiable scores 0.0.
///
/// @param requirement       The first requirement to compare
/// @param other_requirement The second requirement to compare
/// @param step_count        The bound k on trace length for model counting
/// @param metric            Whether to combine counts directly or via logarithm
/// @return                  A semantic similarity score in [0, 1]
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

/// Computes semantic similarity between two specifications by pairing
/// assumptions with assumptions and guarantees with guarantees, by index. The
/// two sides need not be the same length; surplus requirements on either side
/// have no counterpart and are skipped. Pairs whose two requirements compare
/// equal are also skipped, and the score is the mean over the remaining
/// (changed) pairs alone -- identical pairs would otherwise drag the mean
/// toward 1 as the specification grows and hide the requirements a mutation
/// actually touched. Returns 1.0 when no pair differs.
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
