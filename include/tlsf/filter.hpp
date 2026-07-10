#pragma once

/// @file filter.hpp
/// @brief Population filters for tlsf::Specification: deduplication and an
///        assumption-satisfiability guard.

#include "genetic/generation.hpp"
#include "tlsf/specification.hpp"

/// Returns a filter keeping one representative per equal specification (using
/// std::hash / operator== on tlsf::Specification).
FilterFunctionT<tlsf::Specification> tlsf_make_dedup_filter();

/// Returns a filter dropping specifications whose assumption-side conjunction
/// is unsatisfiable — the TLSF analogue of the FRETISH false-condition filter,
/// since contradictory assumptions trivially "realize" any guarantee. A spec
/// with no assumption formulae is kept; an uncertain (timed-out) satisfiability
/// result is treated as satisfiable and the spec is kept.
FilterFunctionT<tlsf::Specification> tlsf_make_assumption_sat_filter();
