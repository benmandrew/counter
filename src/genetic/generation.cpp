#include "genetic/generation.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t k_rate_granularity = 1'000'000;

bool probability_check(double rate, const RandomSource& random_source) {
    if (rate <= 0.0) {
        return false;
    }
    if (rate >= 1.0) {
        return true;
    }
    return random_source.next_index(k_rate_granularity) <
           static_cast<std::size_t>(rate *
                                    static_cast<double>(k_rate_granularity));
}

}  // namespace

FilterFunction make_predicate_filter(
    std::function<bool(const Specification&)> predicate) {
    return [predicate =
                std::move(predicate)](const std::vector<Specification>& pop) {
        std::vector<Specification> survivors;
        survivors.reserve(pop.size());
        for (const Specification& spec : pop) {
            if (predicate(spec)) {
                survivors.push_back(spec);
            }
        }
        return survivors;
    };
}

std::vector<ScoredSpecification> score_population(
    const std::vector<Specification>& population,
    const AggregateWeightedFitnessFunction& fitness_function) {
    assert(!fitness_function.empty());
    const double total_weight = std::accumulate(
        fitness_function.begin(), fitness_function.end(), 0.0,
        [](double acc, const WeightedFitnessFunction& weighted_fn) {
            return acc + weighted_fn.weight;
        });
    assert(total_weight > 0.0);
    std::vector<ScoredSpecification> scored;
    scored.reserve(population.size());
    for (const Specification& spec : population) {
        double weighted_sum = 0.0;
        for (const WeightedFitnessFunction& weighted_fn : fitness_function) {
            weighted_sum += weighted_fn.function(spec) * weighted_fn.weight;
        }
        scored.push_back({spec, weighted_sum / total_weight});
    }
    return scored;
}

std::vector<Specification> filter_population(
    const std::vector<Specification>& population,
    const std::vector<FilterFunction>& filter_functions) {
    std::vector<Specification> current = population;
    for (const FilterFunction& filter_fn : filter_functions) {
        current = filter_fn(current);
    }
    return current;
}

std::vector<ScoredSpecification> evolve_generation(
    const std::vector<ScoredSpecification>& population, std::size_t target_size,
    const AggregateWeightedFitnessFunction& fitness_functions,
    const std::vector<FilterFunction>& filter_functions,
    const EvolutionConfig& config, const RandomSource& random_source) {
    assert(random_source);
    assert(!fitness_functions.empty());
    assert(config.crossover_rate >= 0.0 && config.crossover_rate <= 1.0);
    assert(config.mutation_rate >= 0.0 && config.mutation_rate <= 1.0);

    std::vector<ScoredSpecification> sorted_pop = population;
    std::sort(
        sorted_pop.begin(), sorted_pop.end(),
        [](const ScoredSpecification& lhs, const ScoredSpecification& rhs) {
            return lhs.fitness > rhs.fitness;
        });
    const std::size_t top_n = std::min(target_size, sorted_pop.size());
    std::vector<Specification> next_generation;
    next_generation.reserve(top_n);
    for (std::size_t i = 0; i < top_n; ++i) {
        Specification offspring = sorted_pop[i].specification;
        if (probability_check(config.crossover_rate, random_source)) {
            const std::size_t partner = random_source.next_index(top_n);
            offspring = crossover_specifications(
                offspring, population[partner].specification, random_source);
        }
        if (probability_check(config.mutation_rate, random_source)) {
            offspring = mutate_specification(offspring, random_source);
        }
        next_generation.push_back(std::move(offspring));
    }

    const std::vector<Specification> survivors =
        filter_population(next_generation, filter_functions);
    assert(!survivors.empty());
    std::vector<ScoredSpecification> scored =
        score_population(survivors, fitness_functions);
    std::sort(
        scored.begin(), scored.end(),
        [](const ScoredSpecification& lhs, const ScoredSpecification& rhs) {
            return lhs.fitness > rhs.fitness;
        });

    return scored;
}
