#include "genetic/generation.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t k_rate_granularity = 1'000'000;

bool probability_check(double rate, const RandomSource& random_source) {
    if (rate <= 0.0) return false;
    if (rate >= 1.0) return true;
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
    const std::vector<WeightedFitnessFunction>& fitness_functions) {
    assert(!fitness_functions.empty());
    const double total_weight =
        std::accumulate(fitness_functions.begin(), fitness_functions.end(), 0.0,
                        [](double acc, const WeightedFitnessFunction& wf) {
                            return acc + wf.weight;
                        });
    assert(total_weight > 0.0);
    std::vector<ScoredSpecification> scored;
    scored.reserve(population.size());
    for (const Specification& spec : population) {
        double weighted_sum = 0.0;
        for (const WeightedFitnessFunction& wf : fitness_functions) {
            weighted_sum += wf.function(spec) * wf.weight;
        }
        scored.push_back({spec, weighted_sum / total_weight});
    }
    return scored;
}

std::vector<Specification> filter_population(
    const std::vector<Specification>& population,
    const std::vector<FilterFunction>& filter_functions) {
    std::vector<Specification> current = population;
    for (const FilterFunction& fn : filter_functions) {
        current = fn(current);
    }
    return current;
}

std::vector<Specification> evolve_generation(
    const std::vector<Specification>& population, std::size_t target_size,
    const std::vector<WeightedFitnessFunction>& fitness_functions,
    const std::vector<FilterFunction>& filter_functions,
    const EvolutionConfig& config, const RandomSource& random_source) {
    assert(random_source);
    assert(!fitness_functions.empty());
    assert(config.crossover_rate >= 0.0 && config.crossover_rate <= 1.0);
    assert(config.mutation_rate >= 0.0 && config.mutation_rate <= 1.0);
    const std::vector<Specification> survivors =
        filter_population(population, filter_functions);
    assert(!survivors.empty());
    std::vector<ScoredSpecification> scored =
        score_population(survivors, fitness_functions);
    std::sort(scored.begin(), scored.end(),
              [](const ScoredSpecification& a, const ScoredSpecification& b) {
                  return a.fitness > b.fitness;
              });
    const std::size_t n = std::min(target_size, scored.size());
    std::vector<Specification> next_generation;
    next_generation.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        Specification offspring = scored[i].specification;
        if (probability_check(config.crossover_rate, random_source)) {
            const std::size_t partner = random_source.next_index(n);
            offspring = crossover_specifications(
                offspring, scored[partner].specification, random_source);
        }
        if (probability_check(config.mutation_rate, random_source)) {
            offspring = mutate_specification(offspring, random_source);
        }
        next_generation.push_back(std::move(offspring));
    }
    return next_generation;
}
