#include "genetic/generation.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kSelectionGranularity = 1'000'000;
constexpr std::size_t kRateGranularity = 1'000'000;

bool probability_check(double rate, const RandomSource& random_source) {
    if (rate <= 0.0) return false;
    if (rate >= 1.0) return true;
    return random_source.next_index(kRateGranularity) <
           static_cast<std::size_t>(rate *
                                    static_cast<double>(kRateGranularity));
}

}  // namespace

FilterFunction make_predicate_filter(
    std::function<bool(const Requirement&)> predicate) {
    return [predicate =
                std::move(predicate)](const std::vector<Requirement>& pop) {
        std::vector<Requirement> survivors;
        survivors.reserve(pop.size());
        for (const Requirement& req : pop) {
            if (predicate(req)) {
                survivors.push_back(req);
            }
        }
        return survivors;
    };
}

std::vector<ScoredRequirement> score_population(
    const std::vector<Requirement>& population,
    const std::vector<WeightedFitnessFunction>& fitness_functions) {
    if (fitness_functions.empty()) {
        throw std::invalid_argument(
            "At least one fitness function is required.");
    }
    const double total_weight =
        std::accumulate(fitness_functions.begin(), fitness_functions.end(), 0.0,
                        [](double acc, const WeightedFitnessFunction& wf) {
                            return acc + wf.weight;
                        });
    if (total_weight <= 0.0) {
        throw std::invalid_argument("Total fitness weight must be positive.");
    }
    std::vector<ScoredRequirement> scored;
    scored.reserve(population.size());
    for (const Requirement& req : population) {
        double weighted_sum = 0.0;
        for (const WeightedFitnessFunction& wf : fitness_functions) {
            weighted_sum += wf.function(req) * wf.weight;
        }
        scored.push_back({req, weighted_sum / total_weight});
    }
    return scored;
}

std::vector<Requirement> filter_population(
    const std::vector<Requirement>& population,
    const std::vector<FilterFunction>& filter_functions) {
    std::vector<Requirement> current = population;
    for (const FilterFunction& fn : filter_functions) {
        current = fn(current);
    }
    return current;
}

const Requirement& select_parent(
    const std::vector<ScoredRequirement>& scored_population,
    const RandomSource& random_source) {
    if (scored_population.empty()) {
        throw std::invalid_argument("Cannot select from an empty population.");
    }
    const double total = std::accumulate(
        scored_population.begin(), scored_population.end(), 0.0,
        [](double acc, const ScoredRequirement& s) { return acc + s.fitness; });
    if (total <= 0.0) {
        return scored_population[random_source.next_index(
                                     scored_population.size())]
            .requirement;
    }
    const std::size_t pick = random_source.next_index(kSelectionGranularity);
    double cumulative = 0.0;
    for (const ScoredRequirement& scored : scored_population) {
        cumulative += scored.fitness / total;
        if (pick <
            static_cast<std::size_t>(cumulative * kSelectionGranularity)) {
            return scored.requirement;
        }
    }
    return scored_population.back().requirement;
}

std::vector<Requirement> evolve_generation(
    const std::vector<Requirement>& population, std::size_t target_size,
    const std::vector<WeightedFitnessFunction>& fitness_functions,
    const std::vector<FilterFunction>& filter_functions,
    const EvolutionConfig& config, const RandomSource& random_source) {
    if (!random_source) {
        throw std::invalid_argument("random_source must be callable.");
    }
    if (fitness_functions.empty()) {
        throw std::invalid_argument(
            "At least one fitness function is required.");
    }
    if (config.crossover_rate < 0.0 || config.crossover_rate > 1.0) {
        throw std::invalid_argument("crossover_rate must be in [0, 1].");
    }
    if (config.mutation_rate < 0.0 || config.mutation_rate > 1.0) {
        throw std::invalid_argument("mutation_rate must be in [0, 1].");
    }
    const std::vector<Requirement> survivors =
        filter_population(population, filter_functions);
    if (survivors.empty()) {
        throw std::invalid_argument(
            "All requirements were filtered out; cannot evolve.");
    }
    const std::vector<ScoredRequirement> scored =
        score_population(survivors, fitness_functions);
    std::vector<Requirement> next_generation;
    next_generation.reserve(target_size);
    for (std::size_t i = 0; i < target_size; ++i) {
        const Requirement& first_parent = select_parent(scored, random_source);
        Requirement offspring = first_parent;
        if (probability_check(config.crossover_rate, random_source)) {
            const Requirement& second_parent =
                select_parent(scored, random_source);
            offspring = crossover_requirements(first_parent, second_parent,
                                               random_source);
        }
        if (probability_check(config.mutation_rate, random_source)) {
            offspring = mutate_requirement(offspring, random_source);
        }
        next_generation.push_back(std::move(offspring));
    }
    return next_generation;
}
