#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
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
/// that computes a weighted average of their scores. Results are memoised so
/// each unique specification is scored at most once per instance.
class AggregateWeightedFitnessFunction {
   private:
    std::vector<WeightedFitnessFunction> m_fitness_functions;
    mutable std::unordered_map<Specification, double> m_cache;
    mutable std::unique_ptr<std::mutex> m_cache_mutex =
        std::make_unique<std::mutex>();
    const double m_total_weight;

   public:
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;

    explicit AggregateWeightedFitnessFunction(
        std::vector<WeightedFitnessFunction> fitness_functions)
        : m_fitness_functions(std::move(fitness_functions)),
          m_total_weight([&]() {
              double total = 0.0;
              for (const auto& wff : m_fitness_functions) {
                  total += wff.weight;
              }
              return total;
          }()) {}

    /// Computes the weighted average fitness score for a given specification,
    /// returning a cached value if the specification has been scored before.
    ///
    /// @param spec The specification to score.
    /// @return The weighted average fitness score, or 0.0 if total weight is
    /// not positive.
    double operator()(const Specification& spec) const {
        {
            std::lock_guard<std::mutex> lock(*m_cache_mutex);
            const auto cache_iter = m_cache.find(spec);
            if (cache_iter != m_cache.end()) {
                n_cache_hits++;
                return cache_iter->second;
            }
            n_cache_misses++;
        }
        double weighted_sum = 0.0;
        for (const auto& wff : m_fitness_functions) {
            weighted_sum += wff.weight * wff.function(spec);
        }
        const double result =
            m_total_weight > 0.0 ? weighted_sum / m_total_weight : 0.0;
        std::lock_guard<std::mutex> lock(*m_cache_mutex);
        m_cache.emplace(spec, result);
        return result;
    }

    double total_weight() const { return m_total_weight; }

    /// Checks if the collection of fitness functions is empty.
    [[nodiscard]] bool empty() const { return m_fitness_functions.empty(); }

    [[nodiscard]] auto begin() const { return m_fitness_functions.begin(); }
    [[nodiscard]] auto end() const { return m_fitness_functions.end(); }
};
