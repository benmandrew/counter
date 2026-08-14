#include "fitness/function.hpp"

#include <cstddef>
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

}  // namespace

AggregateWeightedFitnessFunction get_fitness_function(
    const Specification& original_spec, const Config& cfg) {
    std::vector<WeightedFitnessFunction> fitness_functions{};
    if (cfg.fitness_weight_syntactic > 0.0) {
        auto synsim = [original_spec,
                       cfg](const Specification& spec) -> double {
            return syntactic_similarity(spec, original_spec, cfg);
        };
        fitness_functions.push_back(
            {synsim, cfg.fitness_weight_syntactic, "syntactic"});
    }
    if (cfg.fitness_weight_semantic > 0.0) {
        auto semsim = [original_spec,
                       cfg](const Specification& spec) -> double {
            return semantic_similarity(spec, original_spec, cfg);
        };
        fitness_functions.push_back(
            {semsim, cfg.fitness_weight_semantic, "semantic"});
    }
    if (cfg.fitness_weight_status > 0.0) {
        auto status = [grading = cfg.status_grading,
                       slot_order = mrs_slot_order(original_spec, cfg)](
                          const Specification& spec) -> double {
            return specification_status(spec, global_sat_checker(),
                                        global_real_checker(), grading,
                                        slot_order);
        };
        fitness_functions.push_back(
            {status, cfg.fitness_weight_status, "status"});
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
