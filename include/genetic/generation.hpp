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
#include "genetic/pipeline.hpp"
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

   private:
    std::string m_name;
    Fn m_fn;
    mutable std::size_t m_n_in{0};
    mutable std::size_t m_n_out{0};
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
/// A predicate that shells out to a solver costs a whole subprocess per
/// cache-missing candidate, and the miss rate rises with population diversity,
/// so those filters should pass a max_in_flight above 1. Structural predicates
/// are cheaper than a thread-pool dispatch and should leave it at the default.
/// Either way the survivors and their order are identical.
///
/// @param name          Display name used in diagnostic output
/// @param predicate     A predicate returning true for specifications to keep
/// @param max_in_flight Concurrent predicate evaluations; 1 evaluates serially
/// @return              A FilterFunction that applies the predicate
/// element-wise
FilterFunction make_predicate_filter(
    std::string name, std::function<bool(const Specification&)> predicate,
    std::size_t max_in_flight = 1);

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
/// @param cfg               Algorithm configuration (max_scoring_failure_rate)
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
    const std::size_t max_in_flight = dispatch_window();
    std::vector<Scored<Spec>> scored(population.size());
    std::vector<bool> succeeded(population.size(), false);
    std::vector<std::string> errors;
    std::size_t done = 0;
    run_bounded_async(
        population.size(), max_in_flight,
        [&fitness_function, &population](std::size_t idx) {
            return [&fitness_function, &spec = population[idx]] {
                generation_detail::ScoreOutcome outcome;
                try {
                    outcome.result =
                        fitness_function.objectives_and_fitness(spec);
                } catch (const std::exception& exc) {
                    outcome.error = exc.what();
                }
                return outcome;
            };
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

/// Returns the standard set of filter functions used during evolution, in
/// order: deduplication, a bloat cap, a false-condition filter, a vacuity
/// filter (if enabled), a well-separation filter (if enabled), and (if
/// enabled) a weakening filter that keeps only specifications implied by
/// @p original.
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

/// Scores each specification in @p specs and returns them ordered best-first
/// according to @p cfg's selection scheme: descending weighted fitness for
/// WeightedAverage, or the NSGA-II crowded-comparison order for Nsga2 and
/// Nsga2Replicate.
std::vector<ScoredSpecification> score_and_sort_specifications(
    const Config& cfg, const std::vector<Specification>& specs,
    const AggregateWeightedFitnessFunction& fitness_function);

/// Wraps each filter as a named pipeline stage, so a consumer walking the stage
/// list sees one entry per active filter. The filters keep their own n_in/n_out
/// tallies, which the drivers still fold into their end-of-run reports.
///
/// @param filters Captured by reference; must outlive the returned stages.
template <typename Spec>
std::vector<PipelineStage<Spec>> filter_stages(
    const std::vector<FilterFunctionT<Spec>>& filters) {
    std::vector<PipelineStage<Spec>> stages;
    stages.reserve(filters.size());
    for (const FilterFunctionT<Spec>& filter : filters) {
        // Filters built inline for tests carry no display name.
        std::string name = filter.name().empty() ? "filter" : filter.name();
        stages.emplace_back(std::move(name),
                            [&filter](GenerationContext<Spec>& ctx) {
                                ctx.m_candidates = filter(ctx.m_candidates);
                            });
    }
    return stages;
}

/// The names of every stage a generation can run, in pipeline order, counting
/// filters that only run on some generations.
///
/// A consumer needs the full roster up front to reserve a layout that does not
/// move between generations; which stages actually ran in a given generation
/// still comes from the stage reports. Built from the same pipeline the run
/// uses, so it cannot drift from it.
template <typename Spec>
std::vector<std::string> generation_stage_names(
    const std::vector<FilterFunctionT<Spec>>& filters) {
    std::vector<std::string> names;
    for (const PipelineStage<Spec>& stage :
         make_generation_pipeline<Spec>(filter_stages(filters))) {
        names.push_back(stage.name());
    }
    return names;
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
    const GenerationProgressCallback& on_progress = nullptr,
    const StageObserver& on_stage = nullptr) {
    assert(random_source);
    assert(!fitness_functions.empty());
    assert(cfg.crossover_rate >= 0.0 && cfg.crossover_rate <= 1.0);
    assert(cfg.mutation_rate >= 0.0 && cfg.mutation_rate <= 1.0);
    assert(!population.empty());

    GenerationContext<Spec> ctx(
        cfg, population, target_size, elitism_size, ops, random_source,
        [&cfg, &fitness_functions,
         &on_progress](const std::vector<Spec>& candidates) {
            return score_population(cfg, candidates, fitness_functions,
                                    on_progress);
        });
    const std::vector<PipelineStage<Spec>> stages =
        make_generation_pipeline<Spec>(filter_stages(filter_functions));
    return run_generation_pipeline(ctx, stages, on_stage);
}

/// Returns the bundle of FRETISH genetic operators wiring
/// crossover_specifications, mutate_specification, and simplify_offspring for
/// use with evolve_generation_generic.
const GeneticOperators<Specification>& fretish_operators();

/// Evolves a population for one generation:
///   1. Order the population best-first under @p cfg's selection scheme —
///      descending weighted fitness for WeightedAverage, the NSGA-II
///      crowded-comparison order for Nsga2 and Nsga2Replicate — and take the
///      top target_size as parents
///   2. Carry the best elitism_size parents over verbatim as elites (they skip
///      crossover, mutation, and the offspring filters)
///   3. For the remaining parents, apply crossover and mutation to produce
///      offspring
///   4. Apply filter functions sequentially to the offspring to produce
///      survivors, then add the elites back
///   5. Pad survivors back to target_size by duplicating them if filtering
///      reduced the population
///   6. Score the resulting population with fitness functions
///   7. Choose the survivors: under Nsga2 and Nsga2Replicate the parents are
///      pooled with the scored offspring and the best target_size of that
///      union are kept under the crowded-comparison order ((mu+lambda)
///      elitism), Nsga2Replicate deduplicating the pool before ranking it and
///      replicating the distinct survivors back up to target_size; under
///      WeightedAverage the scored population is re-sorted by the weighted
///      scalar
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
/// @param on_stage          Optional callback invoked after each pipeline stage
///                          completes; receives the stage's name, population
///                          sizes, and elapsed time
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
    const GenerationProgressCallback& on_progress = nullptr,
    const StageObserver& on_stage = nullptr);
