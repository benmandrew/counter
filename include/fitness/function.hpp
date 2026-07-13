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
///
/// The memoisation cache retains the individual per-objective scores (one per
/// aggregated function, in registration order), not just the collapsed
/// weighted average. Multi-objective selection (NSGA-II) reads the raw vector
/// via objectives(); the weighted scalar remains available through
/// operator().
template <typename Spec>
class AggregateWeightedFitnessFunctionT {
   private:
    std::vector<WeightedFitnessFunctionT<Spec>> m_fitness_functions;
    mutable std::unordered_map<Spec, std::vector<double>> m_cache;
    mutable std::unique_ptr<std::mutex> m_cache_mutex =
        std::make_unique<std::mutex>();
    const double m_total_weight;

    /// Returns the per-objective scores for @p spec, computing and caching
    /// them on the first request and reusing the cached vector thereafter.
    /// References into m_cache stay valid across later insertions (the cache
    /// is never erased), so returning a copy is safe under concurrency.
    std::vector<double> objectives_cached(const Spec& spec) const {
        {
            std::scoped_lock lock(*m_cache_mutex);
            const auto cache_iter = m_cache.find(spec);
            if (cache_iter != m_cache.end()) {
                n_cache_hits++;
                return cache_iter->second;
            }
            n_cache_misses++;
        }
        std::vector<double> values;
        values.reserve(m_fitness_functions.size());
        for (const auto& wff : m_fitness_functions) {
            values.push_back(wff.function(spec));
        }
        std::scoped_lock lock(*m_cache_mutex);
        return m_cache.emplace(spec, std::move(values)).first->second;
    }

    double weighted_average(const std::vector<double>& objectives) const {
        double weighted_sum = 0.0;
        for (std::size_t i = 0; i < m_fitness_functions.size(); ++i) {
            weighted_sum += m_fitness_functions[i].weight * objectives[i];
        }
        return m_total_weight > 0.0 ? weighted_sum / m_total_weight : 0.0;
    }

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
        return weighted_average(objectives_cached(spec));
    }

    /// Returns the individual per-objective scores for @p spec, in the
    /// registration order of the aggregated fitness functions. Shares the
    /// same memoisation cache as operator().
    std::vector<double> objectives(const Spec& spec) const {
        return objectives_cached(spec);
    }

    /// Scores @p spec once, returning both the raw per-objective vector and
    /// the weighted-average scalar derived from it, with a single cache
    /// lookup. Used by score_population so a scored element carries both.
    std::pair<std::vector<double>, double> objectives_and_fitness(
        const Spec& spec) const {
        std::vector<double> values = objectives_cached(spec);
        const double scalar = weighted_average(values);
        return {std::move(values), scalar};
    }

    /// Number of aggregated objectives (per-element vector length).
    [[nodiscard]] std::size_t n_objectives() const {
        return m_fitness_functions.size();
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
