#pragma once

/// @file operators.hpp
/// @brief Simplification operator and the GeneticOperators bundle wiring
///        crossover, mutation, and simplification for tlsf::Specification.

#include "genetic/operators.hpp"
#include "tlsf/specification.hpp"

/// Simplifies each section formula's propositional subtrees in place via
/// Formula::simplify(), which leaves any temporal nodes (and therefore the
/// temporal skeleton) intact. Returns the simplified specification.
tlsf::Specification tlsf_simplify(tlsf::Specification spec);

/// Returns the process-lifetime bundle of TLSF genetic operators (crossover,
/// mutation, simplification) for use with evolve_generation_generic.
const GeneticOperators<tlsf::Specification>& tlsf_operators();
