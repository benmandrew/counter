#include "fitness/function.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "filter/well_separation.hpp"
#include "fitness/semantic_similarity.hpp"
#include "fitness/status.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"

namespace {

// The admission order for the whole run, computed here rather than at a call
// site because this is the one place that sees the input specification and is
// entered once, before anything is scored. Under MrsAdmissionOrder::Spec it
// costs nothing and returns empty, which the walk reads as index order.
std::vector<std::size_t> mrs_slot_order(const Specification& original_spec,
                                        const Config& cfg) {
    if (cfg.status_grading != StatusGrading::Mrs ||
        cfg.mrs_admission_order != MrsAdmissionOrder::Degree) {
        return {};
    }
    const std::vector<std::size_t> slots =
        live_indices(original_spec.m_guarantees);
    RealizabilityChecker& real = global_real_checker();
    const std::vector<std::size_t> positions = conflict_degree_order(
        slots.size(), [&original_spec, &slots,
                       &real](const std::vector<std::size_t>& indices) {
            Specification subset = original_spec;
            subset.m_guarantees.clear();
            subset.m_guarantees.reserve(indices.size());
            for (const std::size_t index : indices) {
                subset.m_guarantees.push_back(
                    original_spec.m_guarantees[slots[index]]);
            }
            // The same oracle specification_status walks with, undecided
            // resolving as unrealizable in the same direction.
            return real.check_realizability(subset).value_or(false) &&
                   !specification_is_not_well_separated(subset, real);
        });
    // Returned as slots, which survive a guarantee being removed; positions do
    // not. See specification_status.
    std::vector<std::size_t> slot_order;
    slot_order.reserve(positions.size());
    for (const std::size_t position : positions) {
        slot_order.push_back(slots[position]);
    }
    return slot_order;
}

/// The fold every part-wise similarity objective shares: the mean of its terms,
/// with an empty set of terms meaning nothing differed and scoring a perfect
/// match. Both similarity scores define themselves that way.
double mean_or_perfect(const std::vector<double>& values) {
    if (values.empty()) {
        return 1.0;
    }
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

/// What the guarantee-side walk of the status score may spend: one synthesis
/// query per live guarantee, which is the greedy walk's worst case. Only the
/// order this induces over a region's parts is read, so an upper bound is the
/// right shape of estimate.
double status_walk_cost(const Specification& spec) {
    return k_part_cost_synthesis *
           static_cast<double>(live_indices(spec.m_guarantees).size());
}

/// The state a decomposed objective needs, held once for the run rather than
/// copied into every part: the original is a whole Specification and Config is
/// large, and a scoring region builds parts for every candidate in the
/// population.
struct SimilarityContext {
    Specification original;
    Config cfg;
};

struct StatusContext {
    StatusGrading grading;
    std::vector<std::size_t> slot_order;
};

}  // namespace

AggregateWeightedFitnessFunction get_fitness_function(
    const Specification& original_spec, const Config& cfg) {
    std::vector<WeightedFitnessFunction> fitness_functions{};
    const auto similarity = std::make_shared<const SimilarityContext>(
        SimilarityContext{original_spec, cfg});
    if (cfg.fitness_weight_syntactic > 0.0) {
        auto synsim = [similarity](const Specification& spec) -> double {
            return syntactic_similarity(spec, similarity->original,
                                        similarity->cfg);
        };
        // One part, and the cheapest kind there is: this objective never leaves
        // the process, so splitting its arithmetic across workers would cost
        // more in dispatch than it could save. It is still declared rather than
        // left undecomposed, so the launch order knows it is free.
        auto split = [synsim](const Specification& spec) {
            ObjectiveWork work;
            work.parts.push_back({[synsim, &spec] { return synsim(spec); },
                                  k_part_cost_in_process});
            work.combine = [](const std::vector<double>& values) {
                return values.front();
            };
            return work;
        };
        fitness_functions.push_back(
            {synsim, cfg.fitness_weight_syntactic, "syntactic", split});
    }
    if (cfg.fitness_weight_semantic > 0.0) {
        auto semsim = [similarity](const Specification& spec) -> double {
            return semantic_similarity(spec, similarity->original,
                                       similarity->cfg);
        };
        // One part per changed requirement slot, each three bounded model
        // counts over a formula pair independent of every other slot's.
        auto split = [similarity](const Specification& spec) {
            ObjectiveWork work;
            for (std::function<double()>& term : semantic_similarity_terms(
                     spec, similarity->original,
                     similarity->cfg.default_model_counting_bound,
                     similarity->cfg.similarity_metric)) {
                work.parts.push_back(
                    {std::move(term), k_part_cost_model_count});
            }
            work.combine = mean_or_perfect;
            return work;
        };
        fitness_functions.push_back(
            {semsim, cfg.fitness_weight_semantic, "semantic", split});
    }
    if (cfg.fitness_weight_status > 0.0) {
        const auto status_ctx =
            std::make_shared<const StatusContext>(StatusContext{
                cfg.status_grading, mrs_slot_order(original_spec, cfg)});
        auto status = [status_ctx](const Specification& spec) -> double {
            return specification_status(
                spec, global_sat_checker(), global_real_checker(),
                status_ctx->grading, status_ctx->slot_order);
        };
        // One part per component satisfiability query, plus the realizability
        // walk. The walk is handed ComponentCheck::Skipped and the fold applies
        // the component tier from those parts, so no query is asked twice.
        //
        // What that gives up is the walk's short circuit: a candidate with an
        // unsatisfiable component now pays its synthesis queries rather than
        // being graded before they start. The guard cannot be kept without
        // either duplicating every component query or serialising the walk
        // behind them, and the queries it would have saved are the ones
        // RealizabilityChecker memoises most heavily.
        auto split = [status_ctx](const Specification& spec) {
            ObjectiveWork work;
            std::vector<std::string> components =
                specification_status_components(spec);
            const std::size_t n_components = components.size();
            for (std::string& component : components) {
                work.parts.push_back(
                    {[formula = std::move(component)] {
                         return global_sat_checker()
                                        .check_satisfiability(formula)
                                        .value_or(false)
                                    ? 1.0
                                    : k_status_component_unsatisfiable;
                     },
                     k_part_cost_satisfiability});
            }
            work.parts.push_back(
                {[status_ctx, &spec] {
                     return specification_status(
                         spec, global_sat_checker(), global_real_checker(),
                         status_ctx->grading, status_ctx->slot_order,
                         ComponentCheck::Skipped);
                 },
                 status_walk_cost(spec)});
            work.combine = [n_components](const std::vector<double>& values) {
                for (std::size_t i = 0; i < n_components; ++i) {
                    if (values[i] == k_status_component_unsatisfiable) {
                        return k_status_component_unsatisfiable;
                    }
                }
                return values.back();
            };
            return work;
        };
        fitness_functions.push_back(
            {status, cfg.fitness_weight_status, "status", split});
    }
    return AggregateWeightedFitnessFunction(std::move(fitness_functions));
}

std::vector<ScoredSpecification> score_and_sort_specifications(
    const Config& cfg, const std::vector<Specification>& specs,
    const AggregateWeightedFitnessFunction& fitness_function) {
    std::vector<ScoredSpecification> scored;
    scored.reserve(specs.size());
    for (const Specification& spec : specs) {
        auto [objectives, fitness] =
            fitness_function.objectives_and_fitness(spec);
        ScoredSpecification entry;
        entry.specification = spec;
        entry.fitness = fitness;
        entry.objectives = std::move(objectives);
        scored.push_back(std::move(entry));
    }
    order_population(cfg, scored);
    return scored;
}
