#include "genetic/generation.hpp"

#include <algorithm>
#include <cassert>
#include <deque>
#include <future>
#include <numeric>
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

std::vector<ScoredSpecification> score_population(
    const std::vector<Specification>& population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const GenerationProgressCallback& on_progress) {
    assert(!fitness_function.empty());
    const std::size_t max_in_flight =
        Config::n_hw_threads > 0 ? Config::n_hw_threads * 4 : 1;
    std::vector<ScoredSpecification> scored;
    scored.reserve(population.size());
    std::size_t done = 0;
    std::deque<std::pair<const Specification*, std::future<double>>> in_flight;
    for (std::size_t i = 0; i < population.size(); ++i) {
        if (in_flight.size() >= max_in_flight) {
            scored.push_back(
                {*in_flight.front().first, in_flight.front().second.get()});
            in_flight.pop_front();
            if (on_progress) {
                on_progress(++done, population.size());
            }
        }
        in_flight.emplace_back(
            &population[i],
            std::async(std::launch::async,
                       [&fitness_function, &spec = population[i]] {
                           return fitness_function(spec);
                       }));
    }
    while (!in_flight.empty()) {
        scored.push_back(
            {*in_flight.front().first, in_flight.front().second.get()});
        in_flight.pop_front();
        if (on_progress) {
            on_progress(++done, population.size());
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

Specification simplify_offspring(Specification offspring) {
    Specification pre_simplify = offspring;
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
    Specification rededuped(offspring.m_assumptions, offspring.m_guarantees,
                            offspring.m_in_atoms, offspring.m_out_atoms);
    if (rededuped.m_assumptions.size() != pre_simplify.m_assumptions.size() ||
        rededuped.m_guarantees.size() != pre_simplify.m_guarantees.size()) {
        return pre_simplify;
    }
    return rededuped;
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
    assert(!population.empty());

    // Select parents from the whole population, unfiltered: the filter only
    // screens the offspring produced below, after crossover and mutation.
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
        if (probability_check(Config::crossover_rate, random_source)) {
            const std::size_t partner = random_source.next_index(top_n);
            offspring = crossover_specifications(
                offspring, sorted_pop[partner].specification, random_source);
        }
        if (probability_check(Config::mutation_rate, random_source)) {
            offspring = mutate_specification(offspring, random_source);
        }
        next_generation.push_back(simplify_offspring(std::move(offspring)));
    }

    const std::vector<Specification> filtered_offspring =
        filter_population(next_generation, filter_functions);
    assert(!filtered_offspring.empty());

    std::vector<ScoredSpecification> scored =
        score_population(filtered_offspring, fitness_functions, on_progress);
    std::sort(
        scored.begin(), scored.end(),
        [](const ScoredSpecification& lhs, const ScoredSpecification& rhs) {
            return lhs.fitness > rhs.fitness;
        });

    return scored;
}
