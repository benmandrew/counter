#include "genetic/generation.hpp"

#include <algorithm>
#include <cassert>
#include <future>
#include <numeric>
#include <thread>
#include <utility>
#include <vector>

#include "config.hpp"

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

const std::size_t n_hw_threads = std::thread::hardware_concurrency();

std::vector<ScoredSpecification> score_population(
    const std::vector<Specification>& population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const GenerationProgressCallback& on_progress) {
    assert(!fitness_function.empty());
    const std::size_t batch_size =
        n_hw_threads > 0 ? static_cast<std::size_t>(n_hw_threads) * 2 : 1;
    std::vector<ScoredSpecification> scored;
    scored.reserve(population.size());
    std::size_t done = 0;
    for (std::size_t batch_start = 0; batch_start < population.size();
         batch_start += batch_size) {
        const std::size_t batch_end =
            std::min(batch_start + batch_size, population.size());
        std::vector<std::future<double>> futures;
        futures.reserve(batch_end - batch_start);
        for (std::size_t i = batch_start; i < batch_end; ++i) {
            futures.push_back(std::async(
                std::launch::async, [&fitness_function, &spec = population[i]] {
                    return fitness_function(spec);
                }));
        }
        for (std::size_t i = 0; i < futures.size(); ++i) {
            scored.push_back({population[batch_start + i], futures[i].get()});
            if (on_progress) {
                on_progress(++done, population.size());
            }
        }
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
    const RandomSource& random_source,
    const GenerationProgressCallback& on_progress) {
    assert(random_source);
    assert(!fitness_functions.empty());
    assert(Config::crossover_rate >= 0.0 && Config::crossover_rate <= 1.0);
    assert(Config::mutation_rate >= 0.0 && Config::mutation_rate <= 1.0);

    // Filter input population to select eligible parents.
    std::vector<Specification> raw_pop;
    raw_pop.reserve(population.size());
    for (const auto& scored_spec : population) {
        raw_pop.push_back(scored_spec.specification);
    }
    const std::vector<Specification> filtered_specs =
        filter_population(raw_pop, filter_functions);
    assert(!filtered_specs.empty());

    // Reconstruct scored list for eligible parents, preserving existing scores.
    std::vector<ScoredSpecification> eligible;
    eligible.reserve(filtered_specs.size());
    for (const Specification& fspec : filtered_specs) {
        for (const ScoredSpecification& scored : population) {
            if (scored.specification == fspec) {
                eligible.push_back(scored);
                break;
            }
        }
    }

    std::sort(
        eligible.begin(), eligible.end(),
        [](const ScoredSpecification& lhs, const ScoredSpecification& rhs) {
            return lhs.fitness > rhs.fitness;
        });
    const std::size_t top_n = std::min(target_size, eligible.size());
    std::vector<Specification> next_generation;
    next_generation.reserve(top_n);
    for (std::size_t i = 0; i < top_n; ++i) {
        Specification offspring = eligible[i].specification;
        if (probability_check(Config::crossover_rate, random_source)) {
            const std::size_t partner = random_source.next_index(top_n);
            offspring = crossover_specifications(
                offspring, eligible[partner].specification, random_source);
        }
        if (probability_check(Config::mutation_rate, random_source)) {
            offspring = mutate_specification(offspring, random_source);
        }
        for (auto& req : offspring.m_assumptions) {
            req.m_trigger.simplify();
            req.m_response.simplify();
            req.m_ltl = requirement_to_ltl(req);
        }
        for (auto& req : offspring.m_guarantees) {
            req.m_trigger.simplify();
            req.m_response.simplify();
            req.m_ltl = requirement_to_ltl(req);
        }
        next_generation.push_back(std::move(offspring));
    }

    std::vector<ScoredSpecification> scored =
        score_population(next_generation, fitness_functions, on_progress);
    std::sort(
        scored.begin(), scored.end(),
        [](const ScoredSpecification& lhs, const ScoredSpecification& rhs) {
            return lhs.fitness > rhs.fitness;
        });

    return scored;
}
