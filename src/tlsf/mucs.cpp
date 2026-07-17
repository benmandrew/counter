#include "tlsf/mucs.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

#include "runner/spot.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

namespace {

// Section ids for the guarantee (system) side, in the fixed order candidates
// are enumerated. Matches the all_sections() convention in tlsf/fitness.cpp.
constexpr std::size_t k_preset = 1;
constexpr std::size_t k_assert = 4;
constexpr std::size_t k_guarantee = 5;

// The guarantee-side formulae of `spec`, in a stable order (PRESET, then
// ASSERT, then GUARANTEE), each tagged with its section for reconstruction.
std::vector<CoreFormula> guarantee_side_candidates(const Specification& spec) {
    std::vector<CoreFormula> candidates;
    candidates.reserve(spec.m_preset.size() + spec.m_assert.size() +
                       spec.m_guarantee.size());
    for (const Formula& formula : spec.m_preset) {
        candidates.push_back({k_preset, formula});
    }
    for (const Formula& formula : spec.m_assert) {
        candidates.push_back({k_assert, formula});
    }
    for (const Formula& formula : spec.m_guarantee) {
        candidates.push_back({k_guarantee, formula});
    }
    return candidates;
}

// Builds a Specification with `base`'s full environment side (INITIALLY,
// REQUIRE, ASSUME) and metadata, and only `subset` on the guarantee side. With
// `subset` = all of base's guarantee-side formulae this reproduces `base`.
Specification build_candidate_spec(const Specification& base,
                                   const std::vector<CoreFormula>& subset) {
    Specification sub;
    sub.m_title = base.m_title;
    sub.m_description = base.m_description;
    sub.m_semantics = base.m_semantics;
    sub.m_inputs = base.m_inputs;
    sub.m_outputs = base.m_outputs;
    sub.m_initially = base.m_initially;
    sub.m_require = base.m_require;
    sub.m_assume = base.m_assume;
    for (const CoreFormula& entry : subset) {
        switch (entry.section_id) {
            case k_preset:
                sub.m_preset.push_back(entry.formula);
                break;
            case k_assert:
                sub.m_assert.push_back(entry.formula);
                break;
            default:
                sub.m_guarantee.push_back(entry.formula);
                break;
        }
    }
    return sub;
}

bool is_conflict(const Specification& base,
                 const std::vector<CoreFormula>& active,
                 const RealizabilityOracle& is_realizable) {
    return !is_realizable(build_candidate_spec(base, active));
}

std::vector<CoreFormula> concat(std::vector<CoreFormula> lhs,
                                const std::vector<CoreFormula>& rhs) {
    lhs.insert(lhs.end(), rhs.begin(), rhs.end());
    return lhs;
}

// QuickXplain (Junker 2004). `background` is the guarantee-side subset assumed
// present but not under test; `candidates` is the set being minimised.
// `background_grew` records whether the caller just added to `background`, so
// the redundant conflict check on an unchanged background is skipped.
// Returns the minimal subset of `candidates` that, together with `background`,
// is still a conflict (unrealizable).
std::vector<CoreFormula> quickxplain(const Specification& base,
                                     const std::vector<CoreFormula>& background,
                                     bool background_grew,
                                     const std::vector<CoreFormula>& candidates,
                                     const RealizabilityOracle& is_realizable) {
    if (background_grew && is_conflict(base, background, is_realizable)) {
        return {};
    }
    if (candidates.size() == 1) {
        return candidates;
    }
    const auto mid = static_cast<std::ptrdiff_t>(candidates.size() / 2);
    const std::vector<CoreFormula> left(candidates.begin(),
                                        candidates.begin() + mid);
    const std::vector<CoreFormula> right(candidates.begin() + mid,
                                         candidates.end());
    const std::vector<CoreFormula> right_core = quickxplain(
        base, concat(background, left), !left.empty(), right, is_realizable);
    const std::vector<CoreFormula> left_core =
        quickxplain(base, concat(background, right_core), !right_core.empty(),
                    left, is_realizable);
    return concat(left_core, right_core);
}

}  // namespace

MinimalUnrealizableCore extract_muc(const Specification& spec,
                                    const RealizabilityOracle& is_realizable) {
    assert(!is_realizable(spec) &&
           "extract_muc precondition: spec must be unrealizable");
    const std::vector<CoreFormula> candidates = guarantee_side_candidates(spec);
    MinimalUnrealizableCore result;
    if (candidates.empty()) {
        result.spec = build_candidate_spec(spec, {});
        return result;
    }
    result.formulae =
        quickxplain(spec, /*background=*/{},
                    /*background_grew=*/false, candidates, is_realizable);
    result.spec = build_candidate_spec(spec, result.formulae);
    return result;
}

MinimalUnrealizableCore extract_muc(const Specification& spec) {
    RealizabilityChecker& checker = global_real_checker();
    return extract_muc(spec, [&checker](const Specification& candidate) {
        return checker.check_realizability_ltl(
            candidate.to_ltl(), candidate.m_inputs, candidate.m_outputs);
    });
}

const char* section_name(std::size_t section_id) {
    switch (section_id) {
        case 0:
            return "INITIALLY";
        case k_preset:
            return "PRESET";
        case 2:
            return "REQUIRE";
        case 3:
            return "ASSUME";
        case k_assert:
            return "ASSERT";
        case k_guarantee:
            return "GUARANTEE";
        default:
            return "UNKNOWN";
    }
}

}  // namespace tlsf
