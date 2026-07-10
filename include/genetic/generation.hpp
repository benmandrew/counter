#pragma once

/// @file generation.hpp
/// @brief One generation of the genetic repair loop: scoring, filtering,
///        crossover, mutation, and the FilterFunction / ScoredSpecification
///        types.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "config.hpp"
#include "fitness/function.hpp"
#include "genetic/crossover.hpp"
#include "genetic/mutation.hpp"
#include "genetic/operators.hpp"
#include "genetic/random_source.hpp"
#include "genetic/scored.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "thread_pool.hpp"

/// A filter function transforms a population into a surviving subset.
/// Receives the entire population, enabling both per-element predicates and
/// population-level relations such as keeping only maximal elements under a
/// partial order.
///
/// Tracks the input and output population sizes of the most recent invocation
/// via n_in() and n_out(), for per-generation diagnostic output.
template <typename Spec>
class FilterFunctionT {
   public:
    using Fn = std::function<std::vector<Spec>(const std::vector<Spec>&)>;

    FilterFunctionT(std::string name, Fn func)
        : m_name(std::move(name)), m_fn(std::move(func)) {}

    /// Implicit construction from any compatible callable (for tests and
    /// inline construction where a display name is not needed).
    template <
        typename Callable,
        std::enable_if_t<
            !std::is_same_v<std::decay_t<Callable>, FilterFunctionT>, int> = 0>
    FilterFunctionT(  // NOLINT(google-explicit-constructor,runtime/explicit)
        Callable&& func)
        : FilterFunctionT("", Fn(std::forward<Callable>(func))) {}

    std::vector<Spec> operator()(const std::vector<Spec>& pop) const {
        std::vector<Spec> survivors = m_fn(pop);
        m_n_in = pop.size();
        m_n_out = survivors.size();
        return survivors;
    }

    const std::string& name() const { return m_name; }
    std::size_t n_in() const { return m_n_in; }
    std::size_t n_out() const { return m_n_out; }

    /// How often, in generations, this filter runs during evolution
    /// (1 = every generation). Applied by the evolution loop, not here.
    std::size_t interval() const { return m_interval; }
    void set_interval(std::size_t generations) { m_interval = generations; }

   private:
    std::string m_name;
    Fn m_fn;
    mutable std::size_t m_n_in{0};
    mutable std::size_t m_n_out{0};
    std::size_t m_interval{1};
};

/// The FRETISH filter function type.
using FilterFunction = FilterFunctionT<Specification>;

/// Callback invoked after each individual is produced during a generation.
/// @p done is the count produced so far; @p total is the generation size.
using GenerationProgressCallback =
    std::function<void(std::size_t done, std::size_t total)>;

/// A specification paired with its aggregated fitness score.
using ScoredSpecification = Scored<Specification>;

/// Wraps a per-element predicate as a population-level FilterFunction.
///
/// @param name      Display name used in diagnostic output
/// @param predicate A predicate returning true for specifications to keep
/// @return          A FilterFunction that applies the predicate element-wise
FilterFunction make_predicate_filter(
    std::string name, std::function<bool(const Specification&)> predicate);

/// Scores each specification using a weighted average of all fitness functions:
///   fitness = sum(fn_i(spec) * w_i) / sum(w_i)
///
/// @param cfg               Algorithm configuration (parallel thread count)
/// @param population        The population to score
/// @param fitness_function  Non-empty set of weighted fitness functions
/// @param on_progress       Optional callback invoked after each individual is
///                          scored; receives (done, total) counts
/// @return                  Population paired with aggregated fitness scores
/// @throws std::invalid_argument if fitness_function is empty or total weight
///                               is not positive
template <typename Spec, typename Fitness>
std::vector<Scored<Spec>> score_population(
    const Config& cfg, const std::vector<Spec>& population,
    const Fitness& fitness_function,
    const GenerationProgressCallback& on_progress = nullptr) {
    assert(!fitness_function.empty());
    const std::size_t max_in_flight = cfg.parallel > 0 ? cfg.parallel * 4 : 1;
    std::vector<Scored<Spec>> scored(population.size());
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

/// FRETISH overload of score_population. Provided so callers can pass a
/// braced-init-list population (from which the Spec template parameter cannot
/// be deduced); forwards to the generic template.
inline std::vector<ScoredSpecification> score_population(
    const Config& cfg, const std::vector<Specification>& population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const GenerationProgressCallback& on_progress = nullptr) {
    return score_population<Specification, AggregateWeightedFitnessFunction>(
        cfg, population, fitness_function, on_progress);
}

/// Applies filter functions sequentially; each filter receives the survivors
/// from the previous one.
///
/// @param population       The population to filter
/// @param filter_functions Filters applied in order; empty list keeps all
/// @return                 Surviving specifications
template <typename Spec>
std::vector<Spec> filter_population(
    const std::vector<Spec>& population,
    const std::vector<FilterFunctionT<Spec>>& filter_functions) {
    std::vector<Spec> current = population;
    for (const FilterFunctionT<Spec>& filter_fn : filter_functions) {
        current = filter_fn(current);
    }
    return current;
}

/// Returns the standard set of filter functions used during evolution:
/// deduplication, a bloat cap, a false-condition filter, and (if enabled) a
/// weakening filter that keeps only specifications implied by @p original.
/// Each filter's per-generation interval is set from @p cfg; the evolution
/// loop decides which filters run in a given generation.
///
/// @param cfg       Algorithm configuration (filter flags and intervals)
/// @param original  The reference specification for the weakening filter;
///                  captured by value inside the filter
/// @param checker   Satisfiability checker; captured by reference, must
///                  outlive the returned filters
std::vector<FilterFunction> get_filter_functions(
    const Config& cfg, Specification original, SatisfiabilityChecker& checker);

/// Returns the set of filter functions applied to the final realizable
/// population after evolution: deduplication, and (if
/// run_implication_filter) the implication filter.
///
/// @param cfg              Algorithm configuration (run_implication_filter)
/// @param checker          Satisfiability checker for the implication filter;
///                         captured by reference, must outlive the filters
/// @param on_impl_progress Optional progress callback forwarded to the
///                         implication filter
std::vector<FilterFunction> get_final_filter_functions(
    const Config& cfg, SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_impl_progress = nullptr);

/// Selects the filters that should run in a given generation. A filter runs
/// when its interval() divides @p generation (1-indexed), or unconditionally
/// on the final generation so the returned population is never left unfiltered.
///
/// @param filters            All per-generation filters
/// @param generation         1-indexed generation number
/// @param is_last_generation Whether this is the final generation
/// @return                   The subset of @p filters to apply, in order
std::vector<FilterFunction> filters_for_generation(
    const std::vector<FilterFunction>& filters, std::size_t generation,
    bool is_last_generation);

/// Scores each specification in @p specs and returns them sorted by fitness
/// descending.
std::vector<ScoredSpecification> score_and_sort_specifications(
    const std::vector<Specification>& specs,
    const AggregateWeightedFitnessFunction& fitness_function);

namespace generation_detail {

constexpr std::size_t k_rate_granularity = 1'000'000;

inline bool probability_check(double rate, const RandomSource& random_source) {
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

}  // namespace generation_detail

/// Generic one-generation evolution loop, templated on the specification
/// element type @p Spec and any callable fitness type @p Fitness. The three
/// genetic operators (crossover, mutation, simplification) are injected via
/// @p ops rather than hardcoded, so different Spec types supply their own.
///
/// See evolve_generation() for the algorithm; the behaviour is identical.
template <typename Spec, typename Fitness>
std::vector<Scored<Spec>> evolve_generation_generic(
    const Config& cfg, const std::vector<Scored<Spec>>& population,
    std::size_t target_size, const Fitness& fitness_functions,
    const std::vector<FilterFunctionT<Spec>>& filter_functions,
    const GeneticOperators<Spec>& ops, const RandomSource& random_source,
    const GenerationProgressCallback& on_progress = nullptr) {
    assert(random_source);
    assert(!fitness_functions.empty());
    assert(cfg.crossover_rate >= 0.0 && cfg.crossover_rate <= 1.0);
    assert(cfg.mutation_rate >= 0.0 && cfg.mutation_rate <= 1.0);
    assert(!population.empty());

    // Select parents from the whole population, unfiltered: the filter only
    // screens the offspring produced below, after crossover and mutation.
    std::vector<Scored<Spec>> sorted_pop = population;
    std::sort(sorted_pop.begin(), sorted_pop.end(),
              [](const Scored<Spec>& lhs, const Scored<Spec>& rhs) {
                  return lhs.fitness > rhs.fitness;
              });
    const std::size_t top_n = std::min(target_size, sorted_pop.size());
    std::vector<Spec> next_generation;
    next_generation.reserve(top_n);
    for (std::size_t i = 0; i < top_n; ++i) {
        Spec offspring = sorted_pop[i].specification;
        if (generation_detail::probability_check(cfg.crossover_rate,
                                                 random_source)) {
            const std::size_t partner = random_source.next_index(top_n);
            offspring = ops.crossover(
                offspring, sorted_pop[partner].specification, random_source);
        }
        if (generation_detail::probability_check(cfg.mutation_rate,
                                                 random_source)) {
            offspring = ops.mutate(offspring, random_source, cfg);
        }
        next_generation.push_back(ops.simplify
                                      ? ops.simplify(std::move(offspring))
                                      : std::move(offspring));
    }

    std::vector<Spec> filtered_offspring =
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

    std::vector<Scored<Spec>> scored = score_population(
        cfg, filtered_offspring, fitness_functions, on_progress);
    std::sort(scored.begin(), scored.end(),
              [](const Scored<Spec>& lhs, const Scored<Spec>& rhs) {
                  return lhs.fitness > rhs.fitness;
              });

    return scored;
}

/// Returns the bundle of FRETISH genetic operators wiring
/// crossover_specifications, mutate_specification, and simplify_offspring for
/// use with evolve_generation_generic.
const GeneticOperators<Specification>& fretish_operators();

/// Evolves a population for one generation using truncation selection:
///   1. Sort the population by fitness (descending) and take the top
///      target_size as parents
///   2. For each parent, apply crossover and mutation to produce an offspring
///   3. Apply filter functions sequentially to the offspring to produce
///      survivors
///   4. Pad survivors back to target_size by duplicating them if filtering
///      reduced the population
///   5. Score the resulting population with fitness functions
///
/// If the population is smaller than target_size, all of it is used as
/// parents.
///
/// @param cfg               Algorithm configuration (rates and filter flags)
/// @param population        Current generation's specifications
/// @param target_size       Number of offspring to produce
/// @param fitness_function  Non-empty weighted fitness function for scoring
/// @param filter_functions  Filters applied to the offspring after crossover
///                          and mutation, before scoring
/// @param random_source     Random source for crossover and mutation
/// @param on_progress       Optional callback invoked after each individual is
///                          scored; receives (done, total) counts
/// @return                  Next generation of target_size specifications
/// @throws std::invalid_argument if random_source is not callable, if
///                               fitness_function is empty, if rates are
///                               outside [0, 1]
std::vector<ScoredSpecification> evolve_generation(
    const Config& cfg, const std::vector<ScoredSpecification>& population,
    std::size_t target_size,
    const AggregateWeightedFitnessFunction& fitness_function,
    const std::vector<FilterFunction>& filter_functions,
    const RandomSource& random_source,
    const GenerationProgressCallback& on_progress = nullptr);
