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
    /// REQUIRE — environment invariant (G-wrapped in the lowering).
    std::vector<Formula> m_require;
    /// ASSUME / ASSUMPTIONS — environment property, taken verbatim.
    std::vector<Formula> m_assume;
    /// ASSERT / INVARIANT(S) — system invariant (G-wrapped in the lowering).
    std::vector<Formula> m_assert;
    /// GUARANTEE(S) — system property, taken verbatim.
    std::vector<Formula> m_guarantee;

    /// Lowers this specification to a single LTL formula in SPOT syntax.
    ///
    /// This implements the STANDARD-implication lowering. The assumption side
    /// collects, in order and skipping empty sections, `conj(m_initially)`,
    /// `G(conj(m_require))`, and `conj(m_assume)`; the guarantee side collects
    /// `conj(m_preset)`, `G(conj(m_assert))`, and `conj(m_guarantee)`, where
    /// `conj` folds a section with And (empty ⇒ `true`). If the assumption side
    /// contributed no terms the result is just the guarantee conjunction;
    /// otherwise it is `assumptions -> guarantees`.
    ///
    /// Strict semantics (MealyStrict/MooreStrict) are lowered identically to
    /// standard for now — strict implication is a later refinement.
    [[nodiscard]] std::string to_ltl() const;

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
