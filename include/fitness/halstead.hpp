#pragma once

#include <cstddef>

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

/// Collect Halstead token counts from a single propositional formula.
/// Operators are logical connectives (¬, ∧, ∨, →, ↔); operands are atom names.
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
