#pragma once

#include <cstddef>

#include "requirement.hpp"
#include "transfer_matrix.hpp"

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement,
                           std::size_t step_count);

double semantic_similarity(const Requirement& requirement,
                           const Requirement& other_requirement);
