#pragma once

#include <functional>
#include <vector>

#include "genetic/crossover.hpp"
#include "genetic/mutation.hpp"
#include "genetic/random_source.hpp"
#include "requirement.hpp"

/// Default weight for WeightedFitnessFunction when not explicitly specified.
/// Override at compile time with -DCOUNTER_DEFAULT_FITNESS_WEIGHT=<value>.
#ifndef COUNTER_DEFAULT_FITNESS_WEIGHT
inline constexpr double k_default_fitness_weight = 1.0;
#else
inline constexpr double k_default_fitness_weight =
    COUNTER_DEFAULT_FITNESS_WEIGHT;
#endif

/// A fitness function scores a specification, returning a value in [0, 1].
using FitnessFunction = std::function<double(const Specification&)>;

/// A fitness function paired with a weight for weighted-average aggregation.
/// The default weight is given by k_default_fitness_weight.
struct WeightedFitnessFunction {
    FitnessFunction function;
    double weight = k_default_fitness_weight;
};

/// A filter function transforms a population into a surviving subset.
/// Receives the entire population, enabling both per-element predicates and
/// population-level relations such as keeping only maximal elements under a
/// partial order.
using FilterFunction = std::function<std::vector<Specification>(
    const std::vector<Specification>&)>;

/// A specification paired with its aggregated fitness score.
struct ScoredSpecification {
    Specification specification;
    double fitness;
};

/// Parameters controlling stochastic operations within a single generation.
struct EvolutionConfig {
    /// Probability in [0, 1] that an offspring is produced by crossing two
    /// selected parents. When crossover does not fire, the offspring is a copy
    /// of its parent.
    double crossover_rate = 0.1;
    /// Probability in [0, 1] that the result (crossover or copy) is mutated.
    double mutation_rate = 1.0;
};

/// Wraps a per-element predicate as a population-level FilterFunction.
///
/// @param predicate A predicate returning true for specifications to keep
/// @return          A FilterFunction that applies the predicate element-wise
FilterFunction make_predicate_filter(
    std::function<bool(const Specification&)> predicate);

/// Scores each specification using a weighted average of all fitness functions:
///   fitness = sum(fn_i(spec) * w_i) / sum(w_i)
///
/// @param population        The population to score
/// @param fitness_functions Non-empty set of weighted fitness functions
/// @return                  Population paired with aggregated fitness scores
/// @throws std::invalid_argument if fitness_functions is empty or total weight
///                               is not positive
std::vector<ScoredSpecification> score_population(
    const std::vector<Specification>& population,
    const std::vector<WeightedFitnessFunction>& fitness_functions);

/// Applies filter functions sequentially; each filter receives the survivors
/// from the previous one.
///
/// @param population       The population to filter
/// @param filter_functions Filters applied in order; empty list keeps all
/// @return                 Surviving specifications
std::vector<Specification> filter_population(
    const std::vector<Specification>& population,
    const std::vector<FilterFunction>& filter_functions);

/// Evolves a population for one generation using truncation selection:
///   1. Apply filter functions sequentially to produce survivors
///   2. Score survivors with fitness functions
///   3. Sort survivors by fitness (descending) and take the top target_size
///   4. For each of the top candidates, apply crossover and mutation to produce
///      an offspring
///
/// If fewer survivors remain after filtering than target_size, all survivors
/// are used as parents.
///
/// @param population        Current generation's specifications
/// @param target_size       Number of offspring to produce
/// @param fitness_functions Non-empty weighted fitness functions for scoring
/// @param filter_functions  Filters applied to the population before scoring
/// @param config            Crossover and mutation rates
/// @param random_source     Random source for crossover and mutation
/// @return                  Next generation of up to target_size specifications
/// @throws std::invalid_argument if random_source is not callable, if
///                               fitness_functions is empty, if rates are
///                               outside [0, 1], or if the filtered population
///                               is empty
std::vector<Specification> evolve_generation(
    const std::vector<Specification>& population, std::size_t target_size,
    const std::vector<WeightedFitnessFunction>& fitness_functions,
    const std::vector<FilterFunction>& filter_functions,
    const EvolutionConfig& config, const RandomSource& random_source);
