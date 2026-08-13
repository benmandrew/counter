#include "fitness/status.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <vector>

#include "filter/well_separation.hpp"
#include "requirement.hpp"

namespace {

bool all_components_satisfiable(const std::vector<std::string>& components,
                                SatisfiabilityChecker& sat) {
    return std::all_of(
        components.begin(), components.end(),
        [&sat](const std::string& formula) {
            // An unanswered query counts as unsatisfiable,
            // matching the rest of the scoring path: a timeout
            // must not promote a candidate above one that was
            // decided.
            return sat.check_satisfiability(formula).value_or(false);
        });
}

}  // namespace

double status_score(const std::vector<std::string>& components,
                    SatisfiabilityChecker& sat,
                    const std::function<bool()>& is_realizable) {
    if (!all_components_satisfiable(components, sat)) {
        return k_status_component_unsatisfiable;
    }
    return is_realizable() ? k_status_realizable : k_status_unrealizable;
}

double status_score_mrs(const std::vector<std::string>& components,
                        std::size_t n_parts, SatisfiabilityChecker& sat,
                        const SubsetRealizability& subset_realizable) {
    if (!all_components_satisfiable(components, sat)) {
        return k_status_component_unsatisfiable;
    }
    if (n_parts == 0) {
        return k_status_realizable;
    }
    // Grown once and reused across the walk; the oracle reads it and does not
    // retain it. A rejected part is popped, so `kept` is exactly the accepted
    // prefix at every step -- which is what makes the queries recur across
    // near-identical candidates and hit RealizabilityChecker's cache.
    std::vector<std::size_t> kept;
    kept.reserve(n_parts);
    for (std::size_t part = 0; part < n_parts; ++part) {
        kept.push_back(part);
        if (!subset_realizable(kept)) {
            kept.pop_back();
        }
    }
    return static_cast<double>(kept.size()) / static_cast<double>(n_parts);
}

namespace {

// `specification` with only the guarantees at `indices` kept, and its
// assumptions, inputs and outputs unchanged. The environment side is never
// relaxed: weakening an assumption can only make synthesis harder, so it has no
// place in a subset walk looking for what the system can still achieve.
//
// `indices` are positions in the walk, which runs over the live guarantees
// alone; `slots` maps each back to the guarantee vector it came from. Walking
// the raw vector instead would hand a removed guarantee to ltlsynt as a part
// the system has to satisfy.
Specification with_guarantee_subset(const Specification& specification,
                                    const std::vector<std::size_t>& slots,
                                    const std::vector<std::size_t>& indices) {
    Specification subset = specification;
    subset.m_guarantees.clear();
    subset.m_guarantees.reserve(indices.size());
    for (const std::size_t index : indices) {
        assert(index < slots.size());
        subset.m_guarantees.push_back(specification.m_guarantees[slots[index]]);
    }
    return subset;
}

}  // namespace

double specification_status(const Specification& specification,
                            SatisfiabilityChecker& sat,
                            RealizabilityChecker& real, StatusGrading grading) {
    // Requirements are checked one at a time rather than conjoined across the
    // specification: they fire at different times (different conditions,
    // Trigger vs Continual), so their conditions and responses need not be
    // simultaneously satisfiable. Testing `condition & response` per
    // requirement also subsumes testing either half alone, since an
    // unsatisfiable half makes the conjunction unsatisfiable.
    std::vector<std::string> components;
    components.reserve(specification.m_assumptions.size() +
                       specification.m_guarantees.size());
    const auto add = [&components](const std::vector<Requirement>& reqs) {
        for (const Requirement& req : reqs) {
            // A removed requirement is not a component of the specification;
            // scoring its satisfiability would charge a candidate for content
            // it no longer has.
            if (req.m_removed) {
                continue;
            }
            components.push_back("(" + req.m_condition.to_string() + ") & (" +
                                 req.m_response.to_string() + ")");
        }
    };
    add(specification.m_assumptions);
    add(specification.m_guarantees);

    if (grading == StatusGrading::Mrs) {
        const std::vector<std::size_t> slots =
            live_indices(specification.m_guarantees);
        return status_score_mrs(
            components, slots.size(), sat,
            [&specification, &slots,
             &real](const std::vector<std::size_t>& indices) {
                const Specification subset =
                    with_guarantee_subset(specification, slots, indices);
                // Undecided resolves as unrealizable, so the part is rejected:
                // a timed-out query must not buy a candidate a point.
                return real.check_realizability(subset).value_or(false) &&
                       !specification_is_not_well_separated(subset, real);
            });
    }

    return status_score(components, sat, [&specification, &real] {
        // No guarantees leaves the implication with a `true` consequent, which
        // is realizable whatever the assumptions say; skip the solver rather
        // than ask it a question with a known answer.
        const bool realizable =
            count_live(specification.m_guarantees) == 0 ||
            real.check_realizability(specification).value_or(false);
        // Second, and only where the first said yes: a candidate that is
        // already unrealizable cannot be realizable for the wrong reason, and
        // the query is a whole ltlsynt call.
        return realizable &&
               !specification_is_not_well_separated(specification, real);
    });
}
