#pragma once

/// @file specification.hpp
/// @brief The tlsf::Specification type: a basic-TLSF specification decomposed
///        into its six named sections, plus standard-semantics LTL lowering.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "prop_formula.hpp"

namespace tlsf {

/// Synthesis semantics declared in a TLSF INFO block. The machine model
/// (Mealy/Moore) and the implication mode (standard/strict) are orthogonal.
/// Strict semantics are recorded but not yet distinguished by to_ltl(); see the
/// documentation on Specification::to_ltl.
enum class Semantics : std::uint8_t {
    MealyStandard,
    MealyStrict,
    MooreStandard,
    MooreStrict,
};

/// A basic-TLSF specification. The six section vectors mirror the TLSF
/// subsections; each is empty when the corresponding section is absent from the
/// source. Formulae are stored as temporal `Formula` objects (built via the
/// Formula factories, never by parsing temporal strings).
struct Specification {
    std::string m_title;
    std::string m_description;
    Semantics m_semantics = Semantics::MealyStandard;
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_outputs;
    /// INITIALLY — environment initial state.
    std::vector<Formula> m_initially;
    /// PRESET — system initial state.
    std::vector<Formula> m_preset;
    /// REQUIRE / REQUIREMENTS — environment invariant (G-wrapped in lowering).
    std::vector<Formula> m_require;
    /// ASSUME / ASSUMPTIONS — environment property, taken verbatim.
    std::vector<Formula> m_assume;
    /// ASSERT / INVARIANTS — system invariant (G-wrapped in the lowering).
    std::vector<Formula> m_assert;
    /// GUARANTEE(S) — system property, taken verbatim.
    std::vector<Formula> m_guarantee;

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

inline bool operator!=(const Specification& lhs, const Specification& rhs) {
    return !(lhs == rhs);
}

}  // namespace tlsf

/// \cond
namespace std {  // NOLINT(build/namespaces)
template <>
struct hash<tlsf::Specification> {
    std::size_t operator()(const tlsf::Specification& spec) const noexcept {
        auto combine = [](std::size_t seed, std::size_t val) noexcept {
            return seed ^ (val + 0x9e3779b9U + (seed << 6) + (seed >> 2));
        };
        auto fold = [&combine](std::size_t seed,
                               const std::vector<Formula>& section) noexcept {
            for (const Formula& formula : section) {
                seed = combine(seed, std::hash<Formula>{}(formula));
            }
            return seed;
        };
        std::size_t seed = 0;
        for (const std::string& atom : spec.m_inputs) {
            seed = combine(seed, std::hash<std::string>{}(atom));
        }
        for (const std::string& atom : spec.m_outputs) {
            seed = combine(seed, std::hash<std::string>{}(atom));
        }
        seed = fold(seed, spec.m_initially);
        seed = fold(seed, spec.m_preset);
        seed = fold(seed, spec.m_require);
        seed = fold(seed, spec.m_assume);
        seed = fold(seed, spec.m_assert);
        seed = fold(seed, spec.m_guarantee);
        seed = combine(seed, std::hash<std::uint8_t>{}(
                                 static_cast<std::uint8_t>(spec.m_semantics)));
        return seed;
    }
};
}  // namespace std
/// \endcond
