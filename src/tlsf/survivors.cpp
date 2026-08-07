#include "survivors.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "bounded_async.hpp"
#include "config.hpp"
#include "fitness/function.hpp"
#include "genetic/pipeline.hpp"
#include "genetic/scored.hpp"
#include "runner/black.hpp"
#include "thread_pool.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/specification.hpp"

namespace tlsf::internal {

namespace {

// A candidate counts as a repair only when it is realizable and not vacuously
// so. Elites and final-generation offspring reach this collection without
// necessarily having passed the vacuity filter -- that filter can be turned
// off outright -- so the check is repeated here,
// mirroring the FRETISH path's is_realizable_repair, which screens the same
// conditions before any repair is written out. Unconditional on both paths:
// run_vacuity_filter tunes search pressure, never output correctness.
//
// The vacuity check runs first: its syntactic screen is free and its `black`
// queries are per-section-formula or over the assumption side alone, far
// cheaper than the `ltlsynt` query behind tlsf_status, so a vacuous candidate
// is rejected without paying for synthesis.
bool is_tlsf_repair(const Specification& spec, const Config& cfg) {
    return !tlsf_is_vacuous(spec, global_sat_checker()) &&
           tlsf_status(spec, cfg) == 1.0;
}

std::vector<Specification> specifications_of(
    const std::vector<Scored<Specification>>& scored) {
    std::vector<Specification> specs;
    specs.reserve(scored.size());
    for (const Scored<Specification>& candidate : scored) {
        specs.push_back(candidate.specification);
    }
    return specs;
}

// Restricts the survivors to the entries a filter kept, in the original order.
// The filters return bare specifications, so the scores have to be matched back
// on by value.
std::vector<Scored<Specification>> keep_matching(
    const std::vector<Scored<Specification>>& survivors,
    const std::vector<Specification>& kept) {
    std::vector<Scored<Specification>> result;
    result.reserve(kept.size());
    for (const Scored<Specification>& scored : survivors) {
        const bool matched = std::any_of(
            kept.begin(), kept.end(), [&scored](const Specification& spec) {
                return spec == scored.specification;
            });
        if (matched) {
            result.push_back(scored);
        }
    }
    return result;
}

}  // namespace

std::vector<Scored<Specification>> realizable_survivors(
    const std::vector<Scored<Specification>>& population, const Config& cfg,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness) {
    // Each status check is an `ltlsynt` query and the whole final population
    // is checked, so a serial sweep here costs a subprocess per distinct
    // candidate. Verdicts are collected by index and the survivors compacted
    // in population order, so the output matches a serial sweep exactly.
    const std::size_t max_in_flight = dispatch_window();
    std::vector<char> keep(population.size(), 0);
    if (max_in_flight <= 1) {
        for (std::size_t idx = 0; idx < population.size(); ++idx) {
            keep[idx] =
                is_tlsf_repair(population[idx].specification, cfg) ? 1 : 0;
        }
    } else {
        run_bounded_async(
            population.size(), max_in_flight,
            [&population, &cfg](std::size_t idx) {
                return [&spec = population[idx].specification, &cfg] {
                    return is_tlsf_repair(spec, cfg);
                };
            },
            [&keep](std::size_t idx, bool realizable) {
                keep[idx] = realizable ? 1 : 0;
            });
    }
    std::vector<Scored<Specification>> survivors;
    for (std::size_t idx = 0; idx < population.size(); ++idx) {
        if (keep[idx] == 0) {
            continue;
        }
        const Scored<Specification>& scored = population[idx];
        const bool seen =
            std::any_of(survivors.begin(), survivors.end(),
                        [&scored](const Scored<Specification>& kept) {
                            return kept.specification == scored.specification;
                        });
        if (!seen) {
            // Serial on purpose: the final generation scored these same specs,
            // so the fitness cache is warm and each rescore is a cache hit;
            // fanning it out would nest solver dispatch inside solver dispatch
            // for nothing.
            auto [objectives, scalar] =
                fitness.objectives_and_fitness(scored.specification);
            Scored<Specification> survivor;
            survivor.specification = scored.specification;
            survivor.fitness = scalar;
            survivor.objectives = std::move(objectives);
            survivors.push_back(std::move(survivor));
        }
    }
    order_population(cfg, survivors);
    return survivors;
}

std::vector<Scored<Specification>> keep_weakenings(
    const std::vector<Scored<Specification>>& survivors,
    const Specification& original, SatisfiabilityChecker& checker) {
    const std::vector<Specification> specs = specifications_of(survivors);
    const std::vector<Specification> weakenings =
        tlsf_make_weakening_filter(original, checker)(specs);
    return keep_matching(survivors, weakenings);
}

std::vector<Scored<Specification>> keep_maximal(
    const std::vector<Scored<Specification>>& survivors,
    SatisfiabilityChecker& checker) {
    const std::vector<Specification> specs = specifications_of(survivors);
    const std::vector<Specification> maximal =
        tlsf_make_implication_filter(checker)(specs);
    return keep_matching(survivors, maximal);
}

}  // namespace tlsf::internal
