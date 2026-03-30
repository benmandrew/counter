#pragma once

#include <cstddef>

#include "requirement.hpp"
#include "transfer_matrix.hpp"

struct SemanticSimilarityCounts {
    Count m_requirement_count;
    Count m_other_requirement_count;
    Count m_conjunction_count;
};

SemanticSimilarityCounts count_semantic_similarity_terms(
    const Requirement& requirement, const Requirement& other_requirement,
    std::size_t step_count);

double semantic_similarity_from_counts(const SemanticSimilarityCounts& counts);

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count);

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement);
