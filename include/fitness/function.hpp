#pragma once

/// @file function.hpp
/// @brief Fitness function types: FitnessFunction, WeightedFitnessFunction,
///        AggregateWeightedFitnessFunction, and the factory
///        get_fitness_function.

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "config.hpp"
#include "requirement.hpp"

/// Default weight for WeightedFitnessFunction when not explicitly specified.
/// Override at compile time with -DCOUNTER_DEFAULT_FITNESS_WEIGHT=<value>.
#ifndef COUNTER_DEFAULT_FITNESS_WEIGHT
inline constexpr double k_default_fitness_weight = 1.0;
#else
inline constexpr double k_default_fitness_weight =
    COUNTER_DEFAULT_FITNESS_WEIGHT;
#endif

/// A fitness function scores a specification element, returning a value in
/// [0, 1].
template <typename Spec>
using FitnessFunctionT = std::function<double(const Spec&)>;

/// A fitness function paired with a weight for weighted-average aggregation.
/// The default weight is given by k_default_fitness_weight.
template <typename Spec>
struct WeightedFitnessFunctionT {
    FitnessFunctionT<Spec> function;
    double weight = k_default_fitness_weight;
    std::string name;
};

/// Aggregates multiple WeightedFitnessFunctionT instances into a single
/// function that computes a weighted average of their scores. Results are
/// memoised so each unique specification element is scored at most once per
/// instance.
template <typename Spec>
class AggregateWeightedFitnessFunctionT {
   private:
    std::vector<WeightedFitnessFunctionT<Spec>> m_fitness_functions;
    mutable std::unordered_map<Spec, double> m_cache;
    mutable std::unique_ptr<std::mutex> m_cache_mutex =
        std::make_unique<std::mutex>();
    const double m_total_weight;

   public:
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;

    explicit AggregateWeightedFitnessFunctionT(
        std::vector<WeightedFitnessFunctionT<Spec>> fitness_functions)
        : m_fitness_functions(std::move(fitness_functions)),
          m_total_weight([&]() {
              double total = 0.0;
              for (const auto& wff : m_fitness_functions) {
                  total += wff.weight;
              }
              return total;
          }()) {}

    /// Computes the weighted average fitness score for a given specification
    /// element, returning a cached value if it has been scored before.
    ///
    /// @param spec The specification element to score.
    /// @return The weighted average fitness score, or 0.0 if total weight is
    /// not positive.
    double operator()(const Spec& spec) const {
        {
            std::scoped_lock lock(*m_cache_mutex);
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
        std::scoped_lock lock(*m_cache_mutex);
        m_cache.emplace(spec, result);
        return result;
    }

    double total_weight() const { return m_total_weight; }

    /// Checks if the collection of fitness functions is empty.
    [[nodiscard]] bool empty() const { return m_fitness_functions.empty(); }

    [[nodiscard]] auto begin() const { return m_fitness_functions.begin(); }
    [[nodiscard]] auto end() const { return m_fitness_functions.end(); }
};

/// The FRETISH fitness function type aliases.
using FitnessFunction = FitnessFunctionT<Specification>;
using WeightedFitnessFunction = WeightedFitnessFunctionT<Specification>;
using AggregateWeightedFitnessFunction =
    AggregateWeightedFitnessFunctionT<Specification>;

/// Builds the standard set of weighted fitness functions from @p cfg weights.
/// Functions with a zero weight are omitted. The caller owns the returned
/// object and may invoke it repeatedly; results are memoised internally.
///
/// @param original_spec  Reference specification for similarity scoring
/// @param cfg            Configuration providing fitness weights
AggregateWeightedFitnessFunction get_fitness_function(
    const Specification& original_spec, const Config& cfg);
