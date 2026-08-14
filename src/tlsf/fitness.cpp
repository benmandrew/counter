#include "tlsf/fitness.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "fitness/semantic_similarity.hpp"
#include "fitness/status.hpp"
#include "guarantee_parts.hpp"
#include "prop_formula.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/mucs.hpp"

namespace {

using tlsf::Section;
using tlsf::SectionEntry;

constexpr std::size_t k_n_sections = 6;

void collect_atoms(const Formula& formula, std::set<std::string>& out) {
    switch (formula.kind()) {
        case Formula::Kind::Atom: {
            const auto name = formula.atom_name();
            if (name.has_value()) {
                out.insert(*name);
            }
            break;
        }
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally: {
            const auto child = formula.unary_child();
            if (child.has_value()) {
                collect_atoms(*child, out);
            }
            break;
        }
        default: {
            const auto children = formula.binary_children();
            if (children.has_value()) {
                collect_atoms(children->first, out);
                collect_atoms(children->second, out);
            }
            break;
        }
    }
}

// Semantic similarity of a single (changed) formula pair. Counts the bounded
// traces of both formulae and their conjunction over one shared atom universe
// (the union of both formulae's atoms, so the conjunction count never exceeds
// either individual count), then hands the three counts to the shared
// semantic_similarity_from_counts -- the same routine the FRETISH requirement
// pairs use -- so the configured metric (direct or logarithmic) and its [0, 1]
// clamp apply here too.
double formula_pair_semantic_similarity(const Formula& first,
                                        const Formula& second,
                                        std::size_t bound,
                                        SimilarityMetric metric) {
    std::set<std::string> atoms;
    collect_atoms(first, atoms);
    collect_atoms(second, atoms);
    const std::size_t n_atoms = atoms.size();
    if (n_atoms == 0) {
        return first == second ? 1.0 : 0.0;
    }
    const std::string ltl_first = first.to_string();
    const std::string ltl_second = second.to_string();
    const std::string conjunction =
        "(" + ltl_first + ") & (" + ltl_second + ")";
    // Clamp before counting: past max_representable_step_count the products
    // inside count_traces saturate to infinity, and the assert that catches it
    // is compiled out under NDEBUG, so an unclamped bound yields a silently
    // wrong score in release rather than aborting. TLSF has no timing horizon
    // to raise the bound to (the temporal structure is in the formula itself),
    // so the ceiling is the only adjustment.
    const std::size_t step_count =
        std::min(bound, max_representable_step_count(n_atoms));
    // cached_count_traces, not count_traces: the original spec's formulae are
    // re-counted against every offspring in the population, so one shared
    // memo covers the whole run -- and it is the same cache the FRETISH path
    // uses.
    const SemanticSimilarityCounts counts{
        cached_count_traces(ltl_first, n_atoms, step_count),
        cached_count_traces(ltl_second, n_atoms, step_count),
        cached_count_traces(conjunction, n_atoms, step_count)};
    return semantic_similarity_from_counts(counts, metric);
}

}  // namespace

double tlsf_syntactic_similarity(const tlsf::Specification& spec,
                                 const tlsf::Specification& original,
                                 [[maybe_unused]] const Config& cfg) {
    double total = 0.0;
    std::size_t n_pairs = 0;
    const auto spec_sections = tlsf::sections_of(spec);
    const auto original_sections = tlsf::sections_of(original);
    for (std::size_t section = 0; section < k_n_sections; ++section) {
        const Section& lhs = *spec_sections[section];
        const Section& rhs = *original_sections[section];
        const std::size_t paired = std::min(lhs.size(), rhs.size());
        for (std::size_t i = 0; i < paired; ++i) {
            // A conjunct deleted on one side alone has nothing to compare
            // against, and deletion is the largest change a slot admits, so it
            // scores zero. Deleted on both, the slot matches.
            if (lhs[i].m_removed || rhs[i].m_removed) {
                total += lhs[i].m_removed && rhs[i].m_removed ? 1.0 : 0.0;
                continue;
            }
            total += lhs[i].m_formula.syntactic_similarity(rhs[i].m_formula);
        }
        // Missing pairs (the size difference) contribute similarity 0.
        n_pairs += std::max(lhs.size(), rhs.size());
    }
    if (n_pairs == 0) {
        return 1.0;
    }
    return total / static_cast<double>(n_pairs);
}

double tlsf_semantic_similarity(const tlsf::Specification& spec,
                                const tlsf::Specification& original,
                                const Config& cfg) {
    const std::size_t bound = cfg.default_model_counting_bound;
    double total = 0.0;
    std::size_t changed = 0;
    const auto spec_sections = tlsf::sections_of(spec);
    const auto original_sections = tlsf::sections_of(original);
    for (std::size_t section = 0; section < k_n_sections; ++section) {
        const Section& lhs = *spec_sections[section];
        const Section& rhs = *original_sections[section];
        const std::size_t paired = std::min(lhs.size(), rhs.size());
        for (std::size_t i = 0; i < paired; ++i) {
            if (lhs[i] == rhs[i]) {
                continue;
            }
            // Deleted on one side only: a real change, and the largest the slot
            // admits, so it scores zero. There is no formula left to count, and
            // counting the survivor against nothing would spend a model count
            // on an answer already known.
            if (lhs[i].m_removed || rhs[i].m_removed) {
                ++changed;
                continue;
            }
            total += formula_pair_semantic_similarity(lhs[i].m_formula,
                                                      rhs[i].m_formula, bound,
                                                      cfg.similarity_metric);
            ++changed;
        }
    }
    if (changed == 0) {
        return 1.0;
    }
    return total / static_cast<double>(changed);
}

double tlsf_status(const tlsf::Specification& spec, const Config& cfg,
                   const std::vector<std::size_t>& admission_order) {
    SatisfiabilityChecker& sat = global_sat_checker();
    RealizabilityChecker& real = global_real_checker();
    // A TLSF specification's components are the individual formulae of its six
    // sections; the FRETISH path decomposes differently but scores on the same
    // scale, which is why both route through status_score.
    std::vector<std::string> components;
    for (const Section* section : tlsf::sections_of(spec)) {
        for (const SectionEntry& entry : *section) {
            // A deleted conjunct is not a component of the specification;
            // scoring its satisfiability would charge a candidate for content
            // it no longer has.
            if (entry.m_removed) {
                continue;
            }
            components.push_back(entry.m_formula.to_string());
        }
    }
    if (cfg.status_grading == StatusGrading::Mrs) {
        const std::vector<tlsf::CoreFormula> parts =
            tlsf::split_guarantee_parts(spec);
        return status_score_mrs(
            components, parts.size(), sat,
            [&spec, &parts, &real](const std::vector<std::size_t>& indices) {
                const tlsf::Specification subset =
                    tlsf::build_part_subset(spec, parts, indices);
                // Undecided resolves as unrealizable, so the part is rejected:
                // a timed-out query must not buy a candidate a point.
                return real.check_realizability_ltl(subset.to_ltl(),
                                                    subset.m_inputs,
                                                    subset.m_outputs)
                           .value_or(false) &&
                       !tlsf_is_not_well_separated(subset, real);
            },
            admission_order);
    }

    return status_score(components, sat, [&spec, &real] {
        const bool realizable =
            real.check_realizability_ltl(spec.to_ltl(), spec.m_inputs,
                                         spec.m_outputs)
                .value_or(false);
        // Behind the realizability query, as on the FRETISH path: an
        // unrealizable candidate cannot be realizable for the wrong reason.
        return realizable && !tlsf_is_not_well_separated(spec, real);
    });
}

namespace {

// The admission order for the whole run, computed here rather than at a call
// site because this is the one place that sees the specification being evolved
// and is entered once, before anything is scored -- including once per core
// under repair_mode = "muc", where the sub-specification is what gets walked.
// Under MrsAdmissionOrder::Spec it costs nothing and returns empty, which the
// walk reads as index order.
std::vector<std::size_t> tlsf_mrs_admission_order(
    const tlsf::Specification& original, const Config& cfg) {
    if (cfg.status_grading != StatusGrading::Mrs ||
        cfg.mrs_admission_order != MrsAdmissionOrder::Degree) {
        return {};
    }
    const std::vector<tlsf::CoreFormula> parts =
        tlsf::split_guarantee_parts(original);
    RealizabilityChecker& real = global_real_checker();
    return conflict_degree_order(
        parts.size(),
        [&original, &parts, &real](const std::vector<std::size_t>& indices) {
            const tlsf::Specification subset =
                tlsf::build_part_subset(original, parts, indices);
            // The same oracle tlsf_status walks with, undecided resolving as
            // unrealizable in the same direction.
            return real.check_realizability_ltl(
                           subset.to_ltl(), subset.m_inputs, subset.m_outputs)
                       .value_or(false) &&
                   !tlsf_is_not_well_separated(subset, real);
        });
}

}  // namespace

AggregateWeightedFitnessFunctionT<tlsf::Specification>
tlsf_get_fitness_function(const tlsf::Specification& original,
                          const Config& cfg) {
    std::vector<WeightedFitnessFunctionT<tlsf::Specification>> functions;
    if (cfg.fitness_weight_syntactic > 0.0) {
        functions.push_back({[original, cfg](const tlsf::Specification& spec) {
                                 return tlsf_syntactic_similarity(
                                     spec, original, cfg);
                             },
                             cfg.fitness_weight_syntactic, "syntactic"});
    }
    if (cfg.fitness_weight_semantic > 0.0) {
        functions.push_back({[original, cfg](const tlsf::Specification& spec) {
                                 return tlsf_semantic_similarity(spec, original,
                                                                 cfg);
                             },
                             cfg.fitness_weight_semantic, "semantic"});
    }
    if (cfg.fitness_weight_status > 0.0) {
        functions.push_back(
            {[cfg, order = tlsf_mrs_admission_order(original, cfg)](
                 const tlsf::Specification& spec) {
                 return tlsf_status(spec, cfg, order);
             },
             cfg.fitness_weight_status, "status"});
    }
    return AggregateWeightedFitnessFunctionT<tlsf::Specification>(
        std::move(functions));
}
