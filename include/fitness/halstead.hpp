#pragma once

/// @file halstead.hpp
/// @brief Halstead complexity metrics for formulae, requirements, and
///        specifications, used as a size-penalty fitness component.

#include <cstddef>
#include <set>
#include <string>

#include "requirement.hpp"

/// Raw token counts used to derive Halstead complexity measures.
///
/// - eta1: number of distinct operators (logical connectives + timing modality)
/// - eta2: number of distinct operands (atom names + tick counts)
/// - n1:   total operator occurrences
/// - n2:   total operand occurrences
struct HalsteadCounts {
    std::size_t eta1 = 0;
    std::size_t eta2 = 0;
    std::size_t n1 = 0;
    std::size_t n2 = 0;
};

/// The pre-collapse form of HalsteadCounts: the distinct operator and operand
/// *sets* alongside the occurrence totals.
///
/// Aggregating a specification means unioning the distinct sets across its
/// formulae while summing occurrences, which HalsteadCounts alone cannot
/// express — adding two eta1 values counts a shared operator once per formula,
/// so the vocabulary would grow with formula count and inflate the volume.
/// Both the FRETISH and TLSF paths aggregate through this type so they cannot
/// drift apart on that rule.
struct HalsteadTokens {
    std::set<std::string> operators;
    std::set<std::string> operands;
    std::size_t n1 = 0;
    std::size_t n2 = 0;

    /// Unions @p other's distinct sets into this one and sums the occurrences.
    void merge(const HalsteadTokens& other);

    /// Collapses the distinct sets to their cardinalities eta1 and eta2.
    [[nodiscard]] HalsteadCounts to_counts() const;
};

/// Collect raw Halstead tokens from a single formula, for callers that need to
/// aggregate across several formulae before collapsing to a vocabulary size.
HalsteadTokens halstead_tokens(const Formula& formula);

/// Collect Halstead token counts from a single propositional formula.
/// Operators are the logical connectives (¬, ∧, ∨, →, ↔) and the temporal
/// operators (X, F, G, U, R, W); operands are atom names.
HalsteadCounts halstead_counts(const Formula& formula);

/// Collect Halstead token counts from a single requirement, combining the
/// trigger formula, response formula, and timing modality.
/// Parameterized timings (WithinTicks, ForTicks, AfterTicks) contribute both
/// an operator (the modality) and an operand (the tick count).
HalsteadCounts halstead_counts(const Requirement& requirement);

/// Collect Halstead token counts from an entire specification by aggregating
/// all assumptions and guarantees. Distinct operator/operand sets are unioned
/// across requirements; occurrence totals n1 and n2 are summed.
HalsteadCounts halstead_counts(const Specification& specification);

/// Compute Halstead volume V = (n1 + n2) × log₂(η1 + η2).
/// Returns 0.0 when the vocabulary η1 + η2 ≤ 1.
double halstead_volume(const HalsteadCounts& counts);

/// Returns a fitness score in [0, 1] reflecting the Halstead volume of
/// \a specification relative to \a original.
/// A score of 1.0 means the candidate is at most as complex as the original;
/// the score decreases as the candidate's volume exceeds the original's.
double halstead_fitness(const Specification& specification,
                        const Specification& original);
