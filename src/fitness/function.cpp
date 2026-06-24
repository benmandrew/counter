#include "fitness/function.hpp"

#include <algorithm>
#include <utility>
#include <vector>

#include "fitness/halstead.hpp"
#include "fitness/semantic_similarity.hpp"
#include "fitness/status.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"

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
    if (cfg.fitness_weight_halstead > 0.0) {
        auto halstead = [original_spec](const Specification& spec) -> double {
            return halstead_fitness(spec, original_spec);
        };
        fitness_functions.push_back(
            {halstead, cfg.fitness_weight_halstead, "halstead"});
    }
    if (cfg.fitness_weight_status > 0.0) {
        auto status = [](const Specification& spec) -> double {
            return specification_status(spec, global_sat_checker(),
                                        global_real_checker());
        };
        fitness_functions.push_back(
            {status, cfg.fitness_weight_status, "status"});
    }
    return AggregateWeightedFitnessFunction(std::move(fitness_functions));
}

std::vector<ScoredSpecification> score_and_sort_specifications(
    const std::vector<Specification>& specs,
    const AggregateWeightedFitnessFunction& fitness_function) {
    std::vector<ScoredSpecification> scored;
    scored.reserve(specs.size());
    for (const Specification& spec : specs) {
        scored.push_back({spec, fitness_function(spec)});
    }
    std::sort(scored.begin(), scored.end(),
              [](const ScoredSpecification& first,
                 const ScoredSpecification& second) {
                  return first.fitness > second.fitness;
              });
    return scored;
}
