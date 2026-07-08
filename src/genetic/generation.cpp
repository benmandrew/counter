#include "genetic/generation.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "filter/bloat.hpp"
#include "filter/implication.hpp"
#include "thread_pool.hpp"

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
    std::string name, std::function<bool(const Specification&)> predicate) {
    return {std::move(name), [predicate = std::move(predicate)](
                                 const std::vector<Specification>& pop) {
                std::vector<Specification> survivors;
                survivors.reserve(pop.size());
                for (const Specification& spec : pop) {
                    if (predicate(spec)) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}

std::vector<ScoredSpecification> score_population(
    const Config& cfg, const std::vector<Specification>& population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const GenerationProgressCallback& on_progress) {
    assert(!fitness_function.empty());
    const std::size_t max_in_flight = cfg.parallel > 0 ? cfg.parallel * 4 : 1;
    std::vector<ScoredSpecification> scored(population.size());
    std::size_t done = 0;
    run_bounded_async(
        population.size(), max_in_flight,
        [&fitness_function, &population](std::size_t idx) {
            return global_thread_pool().submit(
                [&fitness_function, &spec = population[idx]] {
                    return fitness_function(spec);
                });
        },
        [&scored, &population, &on_progress, &done, total = population.size()](
            std::size_t idx, double fitness) {
            scored[idx] = {population[idx], fitness};
            if (on_progress) {
                on_progress(++done, total);
            }
        });
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
        if (!req.m_weakenable) {
            continue;
        }
        req.m_condition.simplify();
        req.m_response.simplify();
        req.m_ltl = requirement_to_ltl(req);
    }
    for (auto& req : offspring.m_guarantees) {
        if (!req.m_weakenable) {
            continue;
        }
        req.m_condition.simplify();
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
    const Config& cfg, const std::vector<ScoredSpecification>& population,
    std::size_t target_size,
    const AggregateWeightedFitnessFunction& fitness_functions,
    const std::vector<FilterFunction>& filter_functions,
    const RandomSource& random_source,
    const GenerationProgressCallback& on_progress) {
    assert(random_source);
    assert(!fitness_functions.empty());
    assert(cfg.crossover_rate >= 0.0 && cfg.crossover_rate <= 1.0);
    assert(cfg.mutation_rate >= 0.0 && cfg.mutation_rate <= 1.0);
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
        if (probability_check(cfg.crossover_rate, random_source)) {
            const std::size_t partner = random_source.next_index(top_n);
            offspring = crossover_specifications(
                offspring, sorted_pop[partner].specification, random_source);
        }
        if (probability_check(cfg.mutation_rate, random_source)) {
            offspring = mutate_specification(offspring, random_source, cfg);
        }
        next_generation.push_back(simplify_offspring(std::move(offspring)));
    }

    std::vector<Specification> filtered_offspring =
        filter_population(next_generation, filter_functions);
    if (filtered_offspring.empty()) {
        filtered_offspring = std::move(next_generation);
    }
    assert(!filtered_offspring.empty());
    const std::size_t filtered_count = filtered_offspring.size();
    filtered_offspring.reserve(target_size);
    while (filtered_offspring.size() < target_size) {
        filtered_offspring.push_back(
            filtered_offspring[filtered_offspring.size() % filtered_count]);
    }

    std::vector<ScoredSpecification> scored = score_population(
        cfg, filtered_offspring, fitness_functions, on_progress);
    std::sort(
        scored.begin(), scored.end(),
        [](const ScoredSpecification& lhs, const ScoredSpecification& rhs) {
            return lhs.fitness > rhs.fitness;
        });

    return scored;
}

std::vector<FilterFunction> get_filter_functions(
    const Config& cfg, Specification original, SatisfiabilityChecker& checker) {
    std::vector<FilterFunction> filters;
    filters.push_back(make_bloat_cap_filter(original));
    // A false condition is vacuously satisfied by every trace, so it
    // imposes no constraint; forbid it from surviving as a breeding
    // candidate rather than letting the fitness function alone
    // discourage it.
    filters.push_back(
        make_predicate_filter("false-condition", [](const Specification& spec) {
            return !specification_has_false_condition(spec);
        }));
    if (cfg.run_weakening_filter) {
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
        filters.push_back(make_weakening_filter(std::move(original), checker));
    }
    return filters;
}

std::vector<FilterFunction> get_final_filter_functions(
    const Config& cfg, SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_impl_progress) {
    std::vector<FilterFunction> filters;
    filters.push_back(make_dedup_filter());
    if (cfg.run_implication_filter) {
        filters.push_back(make_implication_filter(checker, on_impl_progress));
    }
    return filters;
}
