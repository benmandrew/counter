#pragma once

/// @file filter.hpp
/// @brief Population filters for tlsf::Specification: the TLSF counterparts of
///        the FRETISH deduplication, false-condition (assumption
///        satisfiability), bloat-cap, weakening, and implication filters.

#include <optional>

#include "genetic/generation.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
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

/// Returns a filter dropping specifications that are not well-separated: ones
/// where the system can vacuously satisfy the spec by forcing its own
/// assumptions to fail, i.e. `(assumption-side) -> false` is realizable. The
/// TLSF counterpart of make_well_separation_filter. The ltlsynt query runs only
/// when an assumption-side formula (INITIALLY/REQUIRE/ASSUME) references an
/// output atom; assumptions over inputs alone are well-separated by
/// construction and skip the solver. A timed-out query is treated as
/// unrealizable (well-separated), so a slow check never silently drops a
/// candidate. @p checker is captured by reference and must outlive the filter.
FilterFunctionT<tlsf::Specification> tlsf_make_well_separation_filter(
    RealizabilityChecker& checker);

/// Whether spec @p from logically implies spec @p dest: true when
/// `(from.to_ltl()) & !(dest.to_ltl())` is unsatisfiable, false when
/// satisfiable, nullopt when the black query times out. Unlike the FRETISH
/// assume-guarantee decomposition this is a complete whole-formula check
/// (tlsf::Specification lowers to a single LTL formula via to_ltl()).
std::optional<bool> tlsf_spec_implies(const tlsf::Specification& from,
                                      const tlsf::Specification& dest,
                                      SatisfiabilityChecker& checker);

/// Returns a filter dropping specifications containing any single section
/// formula larger than @p max_ratio times the largest formula in @p original
/// (by Formula::n_subformulae()). The TLSF counterpart of
/// make_bloat_cap_filter.
FilterFunctionT<tlsf::Specification> tlsf_make_bloat_cap_filter(
    const tlsf::Specification& original, double max_ratio = 2.0);

/// Returns a filter keeping only specifications that are logical weakenings of
/// @p original — those that @p original implies (via tlsf_spec_implies). An
/// uncertain (timed-out) check keeps the candidate. The TLSF counterpart of
/// make_weakening_filter. @p checker is captured by reference and must outlive
/// the filter.
FilterFunctionT<tlsf::Specification> tlsf_make_weakening_filter(
    tlsf::Specification original, SatisfiabilityChecker& checker);

/// Returns a filter keeping only the maximal specifications under the
/// implication partial order: spec A strictly dominates B when A implies B but
/// B does not imply A. Mutually equivalent specs are both kept. The TLSF
/// counterpart of make_implication_filter. @p checker is captured by reference
/// and must outlive the filter.
FilterFunctionT<tlsf::Specification> tlsf_make_implication_filter(
    SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress = nullptr);
