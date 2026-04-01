#pragma once

#include <cstddef>

#include "requirement.hpp"

double syntactic_similarity(const Requirement& requirement,
                            const Requirement& other_requirement);
