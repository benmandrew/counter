#include "repair_modes.hpp"

#include <cstddef>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "evolve.hpp"
#include "filter_report.hpp"
#include "fitness/function.hpp"
#include "genetic/accumulator.hpp"
#include "genetic/random_source.hpp"
#include "genetic/scored.hpp"
#include "runner/spot.hpp"
#include "survivors.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/mucs.hpp"
#include "tlsf/specification.hpp"
#include "tlsf/writer.hpp"

namespace tlsf::internal {

namespace {

bool is_realizable(const Specification& spec) {
    // Undecided reads as unrealizable: the repair loop keeps going rather than
    // declaring a specification repaired on a query that never finished.
    return global_real_checker()
        .check_realizability_ltl(spec.to_ltl(), spec.m_inputs, spec.m_outputs,
                                 tlsf::specification_sides(spec))
        .value_or(false);
}

}  // namespace

std::vector<Scored<Specification>> run_monolithic(
    const Specification& original, const Config& cfg,
    const RandomSource& random_source,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness,
    const DashboardProgress& progress, const std::string& output_dir) {
    std::vector<FilterRunStats> filter_stats;
    // The same serialiser repair_N.tlsf goes through, so an accumulated file
    // is a specification document and nothing else -- these are gate-passing
    // candidates, not the run's filtered output.
    RepairAccumulator<Specification> accumulator(
        cfg.accumulate_repairs,
        AccumulatedRepairWriter<Specification>(
            output_dir, ".tlsf",
            [](const Specification& spec) { return write(spec); }));
    const std::vector<Scored<Specification>> population =
        evolve_population(original, cfg, random_source, fitness, filter_stats,
                          progress, accumulator);
    std::vector<Scored<Specification>> survivors =
        realizable_survivors(population, cfg, fitness);
    survivors = merge_accumulated_survivors(
        std::move(survivors), accumulator.specifications(), cfg, fitness);
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
        // Deliberately inert here, whatever cfg.accumulate_repairs says. What
        // this loop evolves is a core sub-specification, so a gate-passing
        // candidate of it is realizable against the core alone -- emitting one
        // would report a fragment as a repair of the whole specification.
        // Reintegrating each with the carried non-core formulae would produce
        // whole specifications, but ones the gate has never seen, so it would
        // cost a second gate sweep over the union rather than nothing.
        RepairAccumulator<Specification> no_accumulation(false);
        const std::vector<Scored<Specification>> population =
            evolve_population(muc.spec, cfg, random_source, sub_fitness,
                              iter_stats, iter_progress, no_accumulation);
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
