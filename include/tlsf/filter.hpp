#pragma once

/// @file filter.hpp
/// @brief Population filters for tlsf::Specification: the TLSF counterparts of
///        the FRETISH deduplication, vacuity, well-separation, bloat-cap,
///        weakening, and implication filters.

#include <optional>
#include <string>
#include <vector>

#include "filter/correctness.hpp"
#include "genetic/generation.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "tlsf/specification.hpp"

/// Returns a filter keeping one representative per equal specification (using
/// std::hash / operator== on tlsf::Specification).
FilterFunctionT<tlsf::Specification> tlsf_make_dedup_filter();

/// Whether @p spec carries a section formula that is a trivial literal: `false`
/// in an assumption section (INITIALLY, REQUIRE, ASSUME), or `true` in a
/// guarantee section (PRESET, ASSERT, GUARANTEE). Either makes
/// `(assumptions) -> (guarantees)` hold for free — the first falsifies the
/// antecedent, the second contributes a no-op conjunct to the consequent.
/// A tlsf::Specification has no condition/response split, so this is where the
/// FRETISH false-condition and true-guarantee tests land on this path.
///
/// Purely syntactic, so it costs no solver call and is checked first. `true`
/// and `false` are ordinary atoms in this AST. Both cases are subsumed by the
/// semantic tests below — a `false` conjunct makes the assumption side
/// unsatisfiable, a `true` one makes a guarantee valid — and are kept as their
/// fast path. Unlike on the FRETISH path, where a `false` *condition* lowers to
/// a satisfiable assumption and nothing semantic rejects it.
bool tlsf_is_trivially_vacuous(const tlsf::Specification& spec);

/// Whether @p spec's assumption-side conjunction (INITIALLY, REQUIRE, ASSUME)
/// is unsatisfiable, making the spec vacuously realizable: a false antecedent
/// turns `(assumptions) -> (guarantees)` into a tautology whatever the
/// guarantees say, so such a spec is not a repair. The TLSF counterpart of
/// specification_has_unsatisfiable_assumptions, and the same check — the two
/// differ only in how they reach the assumption conjunction.
///
/// Conservative under uncertainty: a spec with no assumption formulae, or one
/// whose satisfiability check times out, is reported as not vacuous, so a slow
/// check never silently discards a candidate.
bool tlsf_has_unsatisfiable_assumptions(const tlsf::Specification& spec,
                                        SatisfiabilityChecker& checker);

/// Whether any single guarantee-section formula of @p spec (PRESET, ASSERT,
/// GUARANTEE) is *valid* — its negation unsatisfiable — and so demands nothing
/// of the system. The TLSF counterpart of specification_has_valid_guarantee,
/// including its reasons for splitting the guarantee side per formula while
/// the assumption side stays one joint satisfiability query. ASSERT is
/// G-wrapped by the lowering, but `G psi` is valid exactly when psi is, so the
/// raw section formula is the query. Returns on the first valid formula found;
/// a formula whose check times out is read as falsifiable.
bool tlsf_has_valid_guarantee(const tlsf::Specification& spec,
                              SatisfiabilityChecker& checker);

/// Whether @p spec is vacuous by any of the tests above, cheapest first: the
/// syntactic screen costs nothing, the guarantee check is a small query against
/// a cache keyed per section formula, and the assumption conjunction is one
/// large query whose key changes whenever any assumption mutates. The TLSF
/// counterpart of specification_is_vacuous.
///
/// Shared by the per-generation filter below and the final repair screen in
/// tlsf::run_repair — the filter can be disabled outright, and elites bypass
/// the offspring filters anyway, so the screen cannot rely on it having seen
/// the specifications it is about to write out.
bool tlsf_is_vacuous(const tlsf::Specification& spec,
                     SatisfiabilityChecker& checker);

/// Returns a filter dropping the specifications tlsf_is_vacuous accepts — the
/// TLSF counterpart of make_vacuity_filter, carrying the same "vacuity" stage
/// name so filter reports and dashboard labels line up across the two paths. A
/// spec with no assumption formulae is kept; an uncertain (timed-out)
/// satisfiability result is treated as satisfiable and the spec is kept.
///
/// Gated by Config::run_vacuity_filter, as on the FRETISH path. The final
/// repair screen applies the predicate unconditionally either way, so turning
/// the filter off costs search pressure, never output correctness.
///
/// @param max_in_flight Concurrent checks. Each spec carrying assumptions costs
///                      a `black` subprocess on a cache miss, and the miss rate
///                      rises with population diversity, so a serial sweep here
///                      dominates a diverse run. 1 evaluates serially.
FilterFunctionT<tlsf::Specification> tlsf_make_vacuity_filter(
    std::size_t max_in_flight = 1);

/// Whether @p spec is *not* well-separated: whether the system can vacuously
/// satisfy it by forcing its own assumptions to fail, i.e. whether
/// `(assumption-side) -> false` is realizable. The TLSF counterpart of
/// specification_is_not_well_separated, and shared the same three ways its
/// FRETISH twin is — the per-generation filter, the final gate, and the input
/// screen.
///
/// The ltlsynt query runs only when an assumption-side formula
/// (INITIALLY/REQUIRE/ASSUME) references an output atom; assumptions over
/// inputs alone are well-separated by construction and skip the solver. An
/// undecided query reads as *not* well-separated, inverting the usual reading
/// of a failed synthesis, because here unrealizable is the answer that keeps a
/// candidate.
bool tlsf_is_not_well_separated(const tlsf::Specification& spec,
                                RealizabilityChecker& checker);

/// Returns a filter dropping specifications that are not well-separated: ones
/// where the system can vacuously satisfy the spec by forcing its own
/// assumptions to fail, i.e. `(assumption-side) -> false` is realizable. The
/// TLSF counterpart of make_well_separation_filter. The ltlsynt query runs only
/// when an assumption-side formula (INITIALLY/REQUIRE/ASSUME) references an
/// output atom; assumptions over inputs alone are well-separated by
/// construction and skip the solver. A timed-out query is treated as
/// unrealizable (well-separated), so a slow check never silently drops a
/// candidate. @p checker is captured by reference and must outlive the filter.
///
/// @param checker       Realizability checker for the ltlsynt query; must be
///                      thread-safe when max_in_flight exceeds 1
/// @param max_in_flight Concurrent checks. Each is a full ltlsynt query, itself
///                      gated by Config::max_concurrent_realizability.
FilterFunctionT<tlsf::Specification> tlsf_make_well_separation_filter(
    RealizabilityChecker& checker, std::size_t max_in_flight = 1);

/// Wraps a per-element predicate as a population-level filter, the TLSF
/// counterpart of make_predicate_filter. Verdicts are collected by index and
/// the survivors rebuilt in population order, so a parallel filter drops
/// exactly the same candidates in the same order as a serial one.
///
/// @param name          Display name used in diagnostic output
/// @param predicate     A predicate returning true for specifications to keep
/// @param max_in_flight Concurrent predicate evaluations; 1 evaluates serially
/// @param kind          Whether the fallback may re-admit this filter's rejects
FilterFunctionT<tlsf::Specification> tlsf_make_predicate_filter(
    std::string name, std::function<bool(const tlsf::Specification&)> predicate,
    std::size_t max_in_flight = 1, FilterKind kind = FilterKind::Correctness);

/// The TLSF correctness checks, in the same order and under the same names as
/// correctness_checks on the FRETISH path, and read the same three ways: the
/// per-generation chain, the final gate, and the input screen. See
/// filter/correctness.hpp for why the table is the single source rather than
/// three hand-mirrored lists.
///
/// @param sat  Satisfiability checker (`black`); captured by reference into the
///             returned predicates and must outlive them
/// @param real Realizability checker (`ltlsynt`); likewise
std::vector<CorrectnessCheckT<tlsf::Specification>> tlsf_correctness_checks(
    SatisfiabilityChecker& sat, RealizabilityChecker& real);

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
