#pragma once

/// @file generation.hpp
/// @brief One generation of the genetic repair loop: scoring, filtering,
///        crossover, mutation, and the FilterFunction / ScoredSpecification
///        types.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "config.hpp"
#include "fitness/function.hpp"
#include "genetic/crossover.hpp"
#include "genetic/mutation.hpp"
#include "genetic/nsga2.hpp"
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
/// An individual whose fitness scoring throws is dropped from the returned
/// population rather than aborting the run (see
/// Config::max_scoring_failure_rate). Every drop is tallied here so it is
/// reported at the end of a run: a silent drop must never be mistaken for a
/// clean sweep.
struct ScoringStats {
    // Distinct error messages are capped: a message names the offending
    // formula, so an uncapped tally would grow with the search.
    static constexpr std::size_t k_max_distinct_reasons = 8;

    inline static std::size_t n_dropped = 0;
    inline static std::size_t n_reasons_elided = 0;
    inline static std::map<std::string, std::size_t> reasons;

    static void record(const std::string& reason) {
        n_dropped++;
        const auto found = reasons.find(reason);
        if (found != reasons.end()) {
            found->second++;
        } else if (reasons.size() < k_max_distinct_reasons) {
            reasons.emplace(reason, 1);
        } else {
            n_reasons_elided++;
        }
    }
};

namespace generation_detail {

/// Outcome of one scoring task: the objectives and their weighted scalar, or
/// the message from the fitness function that threw. Failure is carried back
/// as a value rather than left in the future, so a task that fails on one
/// formula cannot unwind the whole scoring pass.
struct ScoreOutcome {
    std::pair<std::vector<double>, double> result;
    std::string error;
};

}  // namespace generation_detail

/// Scores each specification using a weighted average of all fitness functions:
///   fitness = sum(fn_i(spec) * w_i) / sum(w_i)
///
/// An individual whose scoring throws is dropped: the returned population is
/// shorter than @p population, in the same relative order. Above
/// Config::max_scoring_failure_rate of the population the failure is taken to
/// be systematic (a missing or broken external tool) rather than specific to
/// one formula, and the run aborts instead of evolving noise.
///
/// @param cfg               Algorithm configuration (parallel thread count,
///                          max_scoring_failure_rate)
/// @param population        The population to score
/// @param fitness_function  Non-empty set of weighted fitness functions
/// @param on_progress       Optional callback invoked after each individual is
///                          scored; receives (done, total) counts
/// @return                  Successfully scored population paired with their
///                          aggregated fitness scores
/// @throws std::invalid_argument if fitness_function is empty or total weight
///                               is not positive
/// @throws std::runtime_error if more than max_scoring_failure_rate of the
///                            population failed to score
template <typename Spec, typename Fitness>
std::vector<Scored<Spec>> score_population(
    const Config& cfg, const std::vector<Spec>& population,
    const Fitness& fitness_function,
    const GenerationProgressCallback& on_progress = nullptr) {
    assert(!fitness_function.empty());
    const std::size_t max_in_flight = cfg.parallel > 0 ? cfg.parallel * 4 : 1;
    std::vector<Scored<Spec>> scored(population.size());
    std::vector<bool> succeeded(population.size(), false);
    std::vector<std::string> errors;
    std::size_t done = 0;
    run_bounded_async(
        population.size(), max_in_flight,
        [&fitness_function, &population](std::size_t idx) {
            return global_thread_pool().submit(
                [&fitness_function, &spec = population[idx]] {
                    generation_detail::ScoreOutcome outcome;
                    try {
                        outcome.result =
                            fitness_function.objectives_and_fitness(spec);
                    } catch (const std::exception& exc) {
                        outcome.error = exc.what();
                    }
                    return outcome;
                });
        },
        [&scored, &succeeded, &errors, &population, &on_progress, &done,
         total = population.size()](std::size_t idx,
                                    generation_detail::ScoreOutcome outcome) {
            if (outcome.error.empty()) {
                scored[idx].specification = population[idx];
                scored[idx].objectives = std::move(outcome.result.first);
                scored[idx].fitness = outcome.result.second;
                succeeded[idx] = true;
            } else {
                errors.push_back(std::move(outcome.error));
            }
            if (on_progress) {
                on_progress(++done, total);
            }
        });

    // A single failure is tolerated whatever the population size, so a small
    // population is not held to a stricter standard than a large one -- but
    // never the whole population, since evolution cannot continue from nothing.
    const std::size_t tolerated =
        population.empty()
            ? 0
            : std::min(population.size() - 1,
                       std::max<std::size_t>(
                           1, static_cast<std::size_t>(
                                  cfg.max_scoring_failure_rate *
                                  static_cast<double>(population.size()))));
    if (errors.size() > tolerated) {
        throw std::runtime_error(
            "scoring failed for " + std::to_string(errors.size()) + " of " +
            std::to_string(population.size()) + " individuals (tolerating " +
            std::to_string(tolerated) +
            "); the fitness tooling is broken rather than the formulae. First "
            "error: " +
            errors.front());
    }
    for (const std::string& error : errors) {
        ScoringStats::record(error);
    }

    // Compacting by index keeps the surviving order independent of the order
    // the workers happened to finish in, so a fixed RNG seed stays
    // reproducible.
    std::vector<Scored<Spec>> survivors;
    survivors.reserve(population.size() - errors.size());
    for (std::size_t idx = 0; idx < scored.size(); ++idx) {
        if (succeeded[idx]) {
            survivors.push_back(std::move(scored[idx]));
        }
    }
    return survivors;
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

/// Scores each specification in @p specs and returns them ordered best-first
/// according to @p cfg's selection scheme: descending weighted fitness for
/// WeightedAverage, or the NSGA-II crowded-comparison order for Nsga2.
std::vector<ScoredSpecification> score_and_sort_specifications(
    const Config& cfg, const std::vector<Specification>& specs,
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

/// Orders a scored population best-first according to @p cfg's selection
/// scheme: descending weighted fitness for WeightedAverage, or the NSGA-II
/// crowded-comparison order (front rank ascending, crowding descending) for
/// Nsga2. The sort is stable in both cases so a fixed RNG seed is
/// reproducible.
template <typename Spec>
void order_population(const Config& cfg,
                      std::vector<Scored<Spec>>& population) {
    if (cfg.selection_scheme == SelectionScheme::Nsga2) {
        nsga2_sort(population);
        return;
    }
    std::stable_sort(population.begin(), population.end(),
                     [](const Scored<Spec>& lhs, const Scored<Spec>& rhs) {
                         return lhs.fitness > rhs.fitness;
                     });
}

/// Generic one-generation evolution loop, templated on the specification
/// element type @p Spec and any callable fitness type @p Fitness. The three
/// genetic operators (crossover, mutation, simplification) are injected via
/// @p ops rather than hardcoded, so different Spec types supply their own.
///
/// See evolve_generation() for the algorithm; the behaviour is identical.
template <typename Spec, typename Fitness>
std::vector<Scored<Spec>> evolve_generation_generic(
    const Config& cfg, const std::vector<Scored<Spec>>& population,
    std::size_t target_size, std::size_t elitism_size,
    const Fitness& fitness_functions,
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
    order_population(cfg, sorted_pop);
    const std::size_t top_n = std::min(target_size, sorted_pop.size());
    // The best elite_n of the selected parents carry over verbatim (see below);
    // the remaining slots are bred from the top offspring_n parents.
    const std::size_t elite_n = std::min(elitism_size, top_n);
    const std::size_t offspring_n = top_n - elite_n;
    std::vector<Spec> next_generation;
    next_generation.reserve(offspring_n);
    for (std::size_t i = 0; i < offspring_n; ++i) {
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

    std::vector<Spec> survivors =
        filter_population(next_generation, filter_functions);
    if (survivors.empty()) {
        survivors = std::move(next_generation);
    }
    // Elites bypass crossover, mutation, and the offspring filters: the top
    // elite_n specifications carry over unchanged so the best candidates are
    // never lost to a stochastic operator or removed by a filter.
    for (std::size_t i = 0; i < elite_n; ++i) {
        survivors.push_back(sorted_pop[i].specification);
    }
    assert(!survivors.empty());
    const std::size_t survivor_count = survivors.size();
    survivors.reserve(target_size);
    while (survivors.size() < target_size) {
        survivors.push_back(survivors[survivors.size() % survivor_count]);
    }

    std::vector<Scored<Spec>> scored =
        score_population(cfg, survivors, fitness_functions, on_progress);

    if (cfg.selection_scheme == SelectionScheme::Nsga2) {
        // (mu + lambda) survivor selection: pool the incoming parents with the
        // freshly scored offspring, rank the union by the crowded-comparison
        // order, and keep the best target_size. This is NSGA-II's elitism, so
        // no non-dominated candidate is ever lost; padding duplicates carry
        // zero crowding distance and are shed first. Parents keep their cached
        // objective vectors, so pooling adds no re-scoring.
        std::vector<Scored<Spec>> pool = population;
        pool.insert(pool.end(), std::make_move_iterator(scored.begin()),
                    std::make_move_iterator(scored.end()));
        nsga2_sort(pool);
        if (pool.size() > target_size) {
            pool.resize(target_size);
        }
        return pool;
    }

    order_population(cfg, scored);
    return scored;
}

/// Returns the bundle of FRETISH genetic operators wiring
/// crossover_specifications, mutate_specification, and simplify_offspring for
/// use with evolve_generation_generic.
const GeneticOperators<Specification>& fretish_operators();

/// Evolves a population for one generation using truncation selection with
/// elitism:
///   1. Sort the population by fitness (descending) and take the top
///      target_size as parents
///   2. Carry the best elitism_size parents over verbatim as elites (they skip
///      crossover, mutation, and the offspring filters)
///   3. For the remaining parents, apply crossover and mutation to produce
///      offspring
///   4. Apply filter functions sequentially to the offspring to produce
///      survivors, then add the elites back
///   5. Pad survivors back to target_size by duplicating them if filtering
///      reduced the population
///   6. Score the resulting population with fitness functions
///
/// If the population is smaller than target_size, all of it is used as
/// parents. elitism_size is clamped to the number of parents.
///
/// @param cfg               Algorithm configuration (rates and filter flags)
/// @param population        Current generation's specifications
/// @param target_size       Number of offspring to produce
/// @param elitism_size      Number of top parents carried over verbatim;
///                          must be less than target_size
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
    std::size_t target_size, std::size_t elitism_size,
    const AggregateWeightedFitnessFunction& fitness_function,
    const std::vector<FilterFunction>& filter_functions,
    const RandomSource& random_source,
    const GenerationProgressCallback& on_progress = nullptr);
