#include "repair_modes.hpp"

#include <cstddef>
#include <iostream>
#include <utility>
#include <vector>

#include "config.hpp"
#include "evolve.hpp"
#include "filter_report.hpp"
#include "fitness/function.hpp"
#include "genetic/random_source.hpp"
#include "genetic/scored.hpp"
#include "runner/spot.hpp"
#include "survivors.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/mucs.hpp"
#include "tlsf/specification.hpp"

namespace tlsf::internal {

namespace {

bool is_realizable(const Specification& spec) {
    // Undecided reads as unrealizable: the repair loop keeps going rather than
    // declaring a specification repaired on a query that never finished.
    return global_real_checker()
        .check_realizability_ltl(spec.to_ltl(), spec.m_inputs, spec.m_outputs)
        .value_or(false);
}

}  // namespace

std::vector<Scored<Specification>> run_monolithic(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness,
    const DashboardProgress& progress) {
    std::vector<FilterRunStats> filter_stats;
    const std::vector<Scored<Specification>> population = evolve_population(
        original, cfg, random_source, fitness, filter_stats, progress);
    std::vector<Scored<Specification>> survivors =
        realizable_survivors(population, cfg, fitness);
    print_filter_report(filter_stats);
    return survivors;
}

std::vector<Scored<Specification>> run_muc(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& output_fitness,
    const DashboardProgress& progress) {
    std::vector<FilterRunStats> aggregate_stats;
    Specification current = original;
    std::size_t gen_offset = 0;
    for (std::size_t iter = 0; iter < cfg.muc_max_iterations; ++iter) {
        if (is_realizable(current)) {
            break;
        }
        const MinimalUnrealizableCore muc = extract_muc(current);
        if (muc.formulae.empty()) {
            // No guarantee-side core: the environment side alone is
            // unrealizable, which this repair strategy cannot address.
            std::cout << "muc: no guarantee-side core to repair; stopping\n";
            break;
        }
        std::cout << "muc iteration " << (iter + 1) << "/"
                  << cfg.muc_max_iterations << ": core of "
                  << muc.formulae.size() << " formula(s)\n";
        const std::vector<CoreFormula> carried =
            non_core_formulae(current, muc.formulae);
        const AggregateWeightedFitnessFunctionT<Specification> sub_fitness =
            tlsf_get_fitness_function(muc.spec, cfg);
        std::vector<FilterRunStats> iter_stats;
        DashboardProgress iter_progress = progress;
        iter_progress.gen_offset = gen_offset;
        iter_progress.muc_iter = iter + 1;
        const std::vector<Scored<Specification>> population =
            evolve_population(muc.spec, cfg, random_source, sub_fitness,
                              iter_stats, iter_progress);
        gen_offset += cfg.generations;
        accumulate_filter_stats(aggregate_stats, iter_stats);
        const std::vector<Scored<Specification>> sub_survivors =
            realizable_survivors(population, cfg, sub_fitness);
        if (sub_survivors.empty()) {
            std::cout << "muc: core could not be made realizable; stopping\n";
            break;
        }
        current = reintegrate(sub_survivors.front().specification, carried);
    }
    print_filter_report(aggregate_stats);
    if (!is_realizable(current)) {
        return {};
    }
    auto [objectives, scalar] = output_fitness.objectives_and_fitness(current);
    Scored<Specification> repaired;
    repaired.specification = current;
    repaired.fitness = scalar;
    repaired.objectives = std::move(objectives);
    return {std::move(repaired)};
}

}  // namespace tlsf::internal
