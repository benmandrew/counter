#pragma once

/// @file specification.hpp
/// @brief The tlsf::Specification type: a basic-TLSF specification decomposed
///        into its six named sections, plus standard-semantics LTL lowering.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "hash_combine.hpp"
#include "prop_formula.hpp"
#include "runner/spot.hpp"

namespace tlsf {

/// Synthesis semantics declared in a TLSF INFO block. The machine model
/// (Mealy/Moore) and the implication mode (standard/strict) are orthogonal.
/// to_ltl() lowers strict semantics to the weak-until form (PR #27); see the
/// documentation on Specification::to_ltl.
enum class Semantics : std::uint8_t {
    MealyStandard,
    MealyStrict,
    MooreStandard,
    MooreStrict,
};

/// One conjunct of a TLSF section, and whether the search has deleted it.
///
/// A deleted conjunct keeps its slot rather than being erased from the section:
/// crossover requires two parents to have the same section shape, and the
/// similarity objectives pair conjuncts by position, so erasing one would shift
/// every later conjunct and start comparing unrelated formulae against the
/// original. Nothing else in the program may read `m_formula` without first
/// checking `m_removed` — a deleted conjunct is not part of what the
/// specification says.
///
/// The constructor from `Formula` is deliberately implicit, so that parsing and
/// the test fixtures keep building sections from formulae directly; there is
/// deliberately no conversion back, so that every site reading a section has to
/// decide what it does about removal rather than silently ignoring it.
struct SectionEntry {
    Formula m_formula;
    bool m_removed = false;

    // Implicit on purpose: it keeps every site that builds a section out of
    // formulae — the parser and the test fixtures — working unchanged, while
    // the absent conversion back to Formula still forces every *reading* site
    // to decide what it does about m_removed.
    // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
    SectionEntry(Formula formula)  // NOLINT(runtime/explicit)
        : m_formula(std::move(formula)) {}
    SectionEntry(Formula formula, bool removed)
        : m_formula(std::move(formula)), m_removed(removed) {}
};

bool operator==(const SectionEntry& lhs, const SectionEntry& rhs);
bool operator<(const SectionEntry& lhs, const SectionEntry& rhs);

/// A TLSF subsection: an ordered list of conjuncts, some possibly deleted.
using Section = std::vector<SectionEntry>;

/// The formulae of @p section that have not been deleted, in order.
std::vector<Formula> live_formulae(const Section& section);

/// How many conjuncts of @p section have not been deleted.
std::size_t count_live(const Section& section);

/// Positions in @p section of the conjuncts that have not been deleted.
std::vector<std::size_t> live_indices(const Section& section);

/// A basic-TLSF specification. The six sections mirror the TLSF subsections;
/// each is empty when the corresponding section is absent from the source.
/// Formulae are stored as temporal `Formula` objects (built via the Formula
/// factories, never by parsing temporal strings).
struct Specification {
    std::string m_title;
    std::string m_description;
    Semantics m_semantics = Semantics::MealyStandard;
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_outputs;
    /// INITIALLY — environment initial state.
    Section m_initially;
    /// PRESET — system initial state.
    Section m_preset;
    /// REQUIRE / REQUIREMENTS — environment invariant (G-wrapped in lowering).
    Section m_require;
    /// ASSUME / ASSUMPTIONS — environment property, taken verbatim.
    Section m_assume;
    /// ASSERT / INVARIANTS — system invariant (G-wrapped in the lowering).
    Section m_assert;
    /// GUARANTEE(S) — system property, taken verbatim.
    Section m_guarantee;

    /// Lowers this specification to a single LTL formula in SPOT syntax,
    /// following the TLSF v1.1 combination (paper §3.2). With θ_e=INITIALLY,
    /// θ_s=PRESET, ψ_e=REQUIRE, ψ_s=ASSERT, φ_e=ASSUME, φ_s=GUARANTEE (each an
    /// And-fold of its section, absent ⇒ `true` and dropped):
    ///
    ///   standard: `θ_e -> (θ_s & ((G ψ_e & φ_e) -> (G ψ_s & φ_s)))`
    ///   strict:   `θ_e -> (θ_s & (ψ_s W ¬ψ_e) & ((G ψ_e & φ_e) -> φ_s))`
    ///
    /// The strict form (MealyStrict/MooreStrict) requires the system invariant
    /// ψ_s to hold at least as long as the environment invariant ψ_e does.
    ///
    /// Note this nests the initial constraints θ_e/θ_s around the invariant
    /// implication rather than conjoining them flat; assumption_ltl() and
    /// guarantee_ltl() below still return the flat per-side conjunctions, which
    /// are used only for per-side satisfiability checks, not realizability.
    [[nodiscard]] std::string to_ltl() const;

    /// Lowers only the assumption side (INITIALLY, G(REQUIRE), ASSUME) to an
    /// LTL string in SPOT syntax, using the same collection order as to_ltl().
    /// When no assumption section contributes a term the result is `true`.
    [[nodiscard]] std::string assumption_ltl() const;

    /// Lowers only the guarantee side (PRESET, G(ASSERT), GUARANTEE) to an LTL
    /// string in SPOT syntax, using the same collection order as to_ltl().
    /// When no guarantee section contributes a term the result is `true`.
    [[nodiscard]] std::string guarantee_ltl() const;

    friend bool operator==(const Specification& lhs, const Specification& rhs);
    friend bool operator<(const Specification& lhs, const Specification& rhs);
};

/// The six sections in TLSF declaration order. One accessor rather than a copy
/// per translation unit, so that a section cannot be added or its handling
/// changed in one place and missed in four others.
std::array<const Section*, 6> sections_of(const Specification& spec);
std::array<Section*, 6> mutable_sections_of(Specification& spec);

/// The three sections making up the guarantee side (PRESET, ASSERT, GUARANTEE)
/// and the environment side (INITIALLY, REQUIRE, ASSUME) respectively.
std::array<const Section*, 3> guarantee_sections_of(const Specification& spec);
std::array<Section*, 3> mutable_guarantee_sections_of(Specification& spec);
std::array<const Section*, 3> assumption_sections_of(const Specification& spec);

/// The conjunct sets behind `to_ltl()`, for RealizabilityChecker's subsumption
/// table. Every environment conjunct (INITIALLY, REQUIRE, ASSUME) occurs
/// negatively in the lowered formula and every system one (PRESET, ASSERT,
/// GUARANTEE) positively, so strengthening the first side weakens the formula
/// and strengthening the second strengthens it -- the shape the table needs.
/// See RealizabilityChecker::check_realizability_ltl for why REQUIRE, which
/// occurs twice and looks like a counterexample, is not one.
///
/// Each conjunct carries its section as a tag, one formula meaning different
/// things in PRESET and in GUARANTEE, and the scope carries the semantics,
/// strict and non-strict lowering to different shapes. Tombstoned conjuncts
/// are skipped, lowering as an absent section is what deleting them means.
///
/// Valid only for a query over `spec.to_ltl()` itself. The well-separation
/// queries build a different formula and must pass no sides.
SpecificationSides specification_sides(const Specification& spec);
std::array<Section*, 3> mutable_assumption_sections_of(Specification& spec);

/// How many conjuncts of the guarantee side (PRESET, ASSERT, GUARANTEE) have
/// not been deleted. The removal operator's floor is on this: a specification
/// with nothing left to guarantee is realizable by doing nothing.
std::size_t count_live_guarantees(const Specification& spec);

inline bool operator!=(const Specification& lhs, const Specification& rhs) {
    return !(lhs == rhs);
}

}  // namespace tlsf

/// \cond
namespace std {  // NOLINT(build/namespaces)
template <>
struct hash<tlsf::Specification> {
    std::size_t operator()(const tlsf::Specification& spec) const noexcept {
        // The removed flag is hashed alongside the formula, and compared in
        // operator==, because the fitness cache keys on the specification: a
        // deleted conjunct must not collide with the live one it replaced and
        // inherit its score.
        auto fold = [](std::size_t seed,
                       const tlsf::Section& section) noexcept {
            for (const tlsf::SectionEntry& entry : section) {
                seed =
                    hash_combine(seed, std::hash<Formula>{}(entry.m_formula));
                seed = hash_combine(seed, std::hash<bool>{}(entry.m_removed));
            }
            return seed;
        };
        std::size_t seed = 0;
        for (const std::string& atom : spec.m_inputs) {
            seed = hash_combine(seed, std::hash<std::string>{}(atom));
        }
        for (const std::string& atom : spec.m_outputs) {
            seed = hash_combine(seed, std::hash<std::string>{}(atom));
        }
        seed = fold(seed, spec.m_initially);
        seed = fold(seed, spec.m_preset);
        seed = fold(seed, spec.m_require);
        seed = fold(seed, spec.m_assume);
        seed = fold(seed, spec.m_assert);
        seed = fold(seed, spec.m_guarantee);
        seed = hash_combine(seed,
                            std::hash<std::uint8_t>{}(
                                static_cast<std::uint8_t>(spec.m_semantics)));
        return seed;
    }
};
}  // namespace std
/// \endcond
