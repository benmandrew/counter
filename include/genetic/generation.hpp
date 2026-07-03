#pragma once

/// @file generation.hpp
/// @brief One generation of the genetic repair loop: scoring, filtering,
///        crossover, mutation, and the FilterFunction / ScoredSpecification
///        types.

#include <cstddef>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "config.hpp"
#include "fitness/function.hpp"
#include "genetic/crossover.hpp"
#include "genetic/mutation.hpp"
#include "genetic/random_source.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"

/// A filter function transforms a population into a surviving subset.
/// Receives the entire population, enabling both per-element predicates and
/// population-level relations such as keeping only maximal elements under a
/// partial order.
///
/// Tracks the input and output population sizes of the most recent invocation
/// via n_in() and n_out(), for per-generation diagnostic output.
class FilterFunction {
   public:
    using Fn = std::function<std::vector<Specification>(
        const std::vector<Specification>&)>;

    FilterFunction(std::string name, Fn func)
        : m_name(std::move(name)), m_fn(std::move(func)) {}

    /// Implicit construction from any compatible callable (for tests and
    /// inline construction where a display name is not needed).
    template <
        typename Callable,
        std::enable_if_t<
            !std::is_same_v<std::decay_t<Callable>, FilterFunction>, int> = 0>
    FilterFunction(  // NOLINT(google-explicit-constructor,runtime/explicit)
        Callable&& func)
        : FilterFunction("", Fn(std::forward<Callable>(func))) {}

    std::vector<Specification> operator()(
        const std::vector<Specification>& pop) const {
        std::vector<Specification> survivors = m_fn(pop);
        m_n_in = pop.size();
        m_n_out = survivors.size();
        return survivors;
    }

    const std::string& name() const { return m_name; }
    std::size_t n_in() const { return m_n_in; }
    std::size_t n_out() const { return m_n_out; }

   private:
    std::string m_name;
    Fn m_fn;
    mutable std::size_t m_n_in{0};
    mutable std::size_t m_n_out{0};
};

/// Callback invoked after each individual is produced during a generation.
/// @p done is the count produced so far; @p total is the generation size.
using GenerationProgressCallback =
    std::function<void(std::size_t done, std::size_t total)>;

/// A specification paired with its aggregated fitness score.
struct ScoredSpecification {
    Specification specification;
    double fitness = 0.0;
};

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
std::vector<ScoredSpecification> score_population(
    const Config& cfg, const std::vector<Specification>& population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const GenerationProgressCallback& on_progress = nullptr);

/// Applies filter functions sequentially; each filter receives the survivors
/// from the previous one.
///
/// @param population       The population to filter
/// @param filter_functions Filters applied in order; empty list keeps all
/// @return                 Surviving specifications
std::vector<Specification> filter_population(
    const std::vector<Specification>& population,
    const std::vector<FilterFunction>& filter_functions);

/// Returns the standard set of filter functions used during evolution,
/// including a weakening filter that keeps only specifications implied by
/// @p original.
///
/// @param cfg       Algorithm configuration (run_weakening_filter flag)
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

/// Scores each specification in @p specs and returns them sorted by fitness
/// descending.
std::vector<ScoredSpecification> score_and_sort_specifications(
    const std::vector<Specification>& specs,
    const AggregateWeightedFitnessFunction& fitness_function);

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
