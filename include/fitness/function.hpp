#pragma once

#include <functional>
#include <vector>

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

/// Aggregates multiple WeightedFitnessFunction instances into a single function
/// that computes a weighted average of their scores.
class AggregateWeightedFitnessFunction {
   private:
    std::vector<WeightedFitnessFunction> m_fitness_functions;

   public:
    explicit AggregateWeightedFitnessFunction(
        const std::vector<WeightedFitnessFunction>& fitness_functions)
        : m_fitness_functions(fitness_functions) {}

    /// Computes the weighted average fitness score for a given specification.
    ///
    /// @param spec The specification to score.
    /// @return The weighted average fitness score, or 0.0 if total weight is
    /// not positive.
    double operator()(const Specification& spec) const {
        double total_weight = 0.0;
        double weighted_sum = 0.0;
        for (const auto& wff : m_fitness_functions) {
            double score = wff.function(spec);
            weighted_sum += wff.weight * score;
            total_weight += wff.weight;
        }
        return total_weight > 0.0 ? weighted_sum / total_weight : 0.0;
    }

    /// Checks if the collection of fitness functions is empty.
    bool empty() const { return m_fitness_functions.empty(); }

    auto begin() const { return m_fitness_functions.begin(); }
    auto end() const { return m_fitness_functions.end(); }
};
