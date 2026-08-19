#include "evolution.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "bounded_async.hpp"
#include "filter/correctness.hpp"
#include "filter/implication.hpp"
#include "fitness/status.hpp"
#include "genetic/accumulator.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
#include "serialisation.hpp"
#include "status_line.hpp"
#include "thread_pool.hpp"

namespace {

// The correctness checks, built once. Their predicates capture the global
// checkers, which outlive every caller.
const std::vector<CorrectnessCheck>& gate_checks() {
    static const std::vector<CorrectnessCheck> checks =
        correctness_checks(global_sat_checker(), global_real_checker());
    return checks;
}

// A specification counts as a realizable repair only if it is realizable and
// passes every correctness check. Elites and the seed population reach this
// unscreened, and the per-generation flags can turn any of those checks off
// outright, so both the live "real" counter and the final collection apply the
// whole table here, unconditionally.
//
// Status leads. Most of the final population is unrealizable -- that is why the
// collection drops most of it -- and status is a scored objective, so its
// verdict is already memoised for anything the last generation scored, whereas
// the checks behind it are only warm where their per-generation stage ran.
// Asking the warm question first short-circuits the common case for free.
bool is_realizable_repair(const Specification& spec) {
    return specification_status(spec, global_sat_checker(),
                                global_real_checker()) == 1.0 &&
           !first_failing_check(spec, gate_checks()).has_value();
}

}  // namespace

std::vector<std::string> fitness_objective_names(
    const AggregateWeightedFitnessFunction& fitness_function) {
    std::vector<std::string> names;
    for (const WeightedFitnessFunction& objective : fitness_function) {
        names.push_back(objective.name);
    }
    return names;
}

std::vector<ScoredSpecification> original_population(
    Specification& original_spec,
    const AggregateWeightedFitnessFunction& fitness_function,
    std::size_t population_size) {
    std::vector<ScoredSpecification> population;
    population.reserve(population_size);
    auto [objectives, fitness] =
        fitness_function.objectives_and_fitness(original_spec);
    for (std::size_t i = 0; i < population_size; ++i) {
        ScoredSpecification scored;
        scored.specification = original_spec;
        scored.fitness = fitness;
        scored.objectives = objectives;
        population.push_back(std::move(scored));
    }
    return population;
}

EvolutionResult run_evolution(
    const Config& cfg, std::vector<ScoredSpecification> population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const std::vector<FilterFunction>& filter_functions,
    RandomSource& random_source, DashboardWriter& dashboard,
    const std::string& output_dir) {
    // The same serialiser repair_N.json goes through, so an accumulated file
    // is a specification document and nothing else -- no fitness record, since
    // these are gate-passing candidates rather than the run's filtered output.
    RepairAccumulator<Specification> accumulator(
        cfg.accumulate_repairs,
        AccumulatedRepairWriter<Specification>(
            output_dir, ".json", [](const Specification& spec) {
                const nlohmann::json jobj = spec;
                return jobj.dump(2) + "\n";
            }));
    const std::vector<std::string> objective_names =
        fitness_objective_names(fitness_function);
    std::vector<FilterRunStats> filter_stats;
    filter_stats.reserve(filter_functions.size());
    for (const FilterFunction& flt : filter_functions) {
        filter_stats.push_back({flt.name(), 0, 0});
    }
    StatusLine status;
    const std::size_t col_gen = status.add("gen");
    // Transient: within-generation progress is the reason the line updates at
    // all, and reads 100% on every committed line by construction.
    const std::size_t col_pct = status.add("%", true);
    const std::size_t col_time = status.add("time");
    const std::size_t col_best = status.add("best");
    const std::size_t col_real = status.add("real");

    auto format_elapsed = [](double secs) -> std::string {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << secs << "s";
        return oss.str();
    };

    const std::size_t pop_size = population.size();
    const std::size_t selection_size = std::max(
        std::size_t{1}, static_cast<std::size_t>(static_cast<double>(pop_size) *
                                                 cfg.selection_rate));
    // Elites carry over verbatim; unlike selection there is no floor of 1, so
    // a small population or rate can legitimately yield no elites. The config
    // guarantees elitism_rate < selection_rate, keeping this below
    // selection_size.
    const auto elitism_size = static_cast<std::size_t>(
        static_cast<double>(pop_size) * cfg.elitism_rate);
    const std::string total_str = std::to_string(cfg.generations);
    for (std::size_t gen_idx = 0; gen_idx < cfg.generations; ++gen_idx) {
        const auto start = std::chrono::steady_clock::now();
        const std::string gen_str =
            std::to_string(gen_idx + 1) + "/" + total_str;
        status.set(col_gen, gen_str);

        auto on_progress = [&](std::size_t done, std::size_t total) {
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            status.set(col_pct, std::to_string(done * 100 / total) + "%");
            status.set(col_time, format_elapsed(elapsed));
            status.render();
        };

        // The stage index is assigned here rather than by the pipeline: the
        // observer sees stages in order, and the dashboard needs their order
        // without the pipeline having to number them.
        std::size_t stage_index = 0;
        auto on_stage = [&dashboard, &stage_index,
                         gen = gen_idx + 1](const StageObservation& obs) {
            dashboard.stage(gen, stage_index++, obs);
        };

        population = evolve_generation(
            cfg, population, selection_size, elitism_size, fitness_function,
            filter_functions, random_source, on_progress, on_stage);

        // Each active filter copy carries this generation's in/out sizes; fold
        // them into the running per-filter totals reported at the end.
        for (const FilterFunction& flt : filter_functions) {
            for (FilterRunStats& stat : filter_stats) {
                if (stat.name == flt.name()) {
                    stat.total_in += flt.n_in();
                    stat.total_out += flt.n_out();
                    break;
                }
            }
        }

        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        status.set(col_time, format_elapsed(elapsed));

        // The maximum, not population[0]: under NSGA-II the population is
        // ordered by front rank and crowding distance, so the first individual
        // need not hold the highest weighted scalar. Reporting it as "best"
        // would put a number on the dashboard below the mean beside it -- and
        // put one on the status line that falls between generations while the
        // search is still improving, which reads as the search going backwards.
        // Computed before the status line rather than after so both report it.
        double fitness_total = 0.0;
        double fitness_best = 0.0;
        std::vector<std::vector<double>> objectives;
        objectives.reserve(population.size());
        for (const ScoredSpecification& cand : population) {
            fitness_total += cand.fitness;
            fitness_best = std::max(fitness_best, cand.fitness);
            objectives.push_back(cand.objectives);
        }
        if (!population.empty()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << fitness_best;
            status.set(col_best, oss.str());
        }
        std::size_t n_real = 0;
        for (const ScoredSpecification& cand : population) {
            if (is_realizable_repair(cand.specification)) {
                ++n_real;
                // Free: this is the gate query the status line needs anyway,
                // so accumulating here costs a hash insertion and no solver
                // call. A second gate call would not be.
                accumulator.insert(cand.specification, gen_idx + 1);
            }
        }
        status.set(col_real, std::to_string(n_real));
        status.finish();

        dashboard.generation(
            gen_idx + 1, elapsed, fitness_best,
            population.empty()
                ? 0.0
                : fitness_total / static_cast<double>(population.size()),
            mean_objectives(objective_names, objectives), n_real,
            population.size());
    }
    return {std::move(population), std::move(filter_stats),
            accumulator.specifications()};
}

std::vector<Specification> collect_realizable_specifications(
    const std::vector<ScoredSpecification>& population) {
    // Each check is a `black` and an `ltlsynt` query, and the whole final
    // population is checked, so a serial sweep here costs a subprocess per
    // distinct candidate -- the more diverse the population, the worse. Results
    // are collected by index and the survivors rebuilt in population order, so
    // the output matches a serial sweep exactly.
    const std::size_t max_in_flight = dispatch_window();
    std::vector<char> keep(population.size(), 0);
    // The per-generation filter only screens offspring during evolution, so a
    // vacuous result from the final generation would otherwise never be
    // re-screened before being reported here. Elites bypass the offspring
    // filters entirely, so one can reach the output unscreened either way.
    if (max_in_flight <= 1) {
        for (std::size_t idx = 0; idx < population.size(); ++idx) {
            keep[idx] =
                is_realizable_repair(population[idx].specification) ? 1 : 0;
        }
    } else {
        run_bounded_async(
            population.size(), max_in_flight,
            [&population](std::size_t idx) {
                return [&spec = population[idx].specification] {
                    return is_realizable_repair(spec);
                };
            },
            [&keep](std::size_t idx, bool realizable) {
                keep[idx] = realizable ? 1 : 0;
            });
    }
    std::vector<Specification> realizable_vec;
    for (std::size_t idx = 0; idx < population.size(); ++idx) {
        if (keep[idx] != 0) {
            realizable_vec.push_back(population[idx].specification);
        }
    }
    return realizable_vec;
}

std::pair<std::vector<Specification>, std::vector<FilterRunStats>>
filter_maximal_specifications(
    const Config& cfg, const Specification& original,
    const std::vector<Specification>& realizable_vec) {
    const auto impl_start = std::chrono::steady_clock::now();
    auto on_impl_progress = [&impl_start](std::size_t done, std::size_t total) {
        // Off a terminal this frame is never overwritten, so it would land in
        // the log once per comparison; the committed line below says the same
        // thing once.
        if (!stdout_is_tty()) {
            return;
        }
        const double elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          impl_start)
                .count();
        std::cout << "\r\033[KImplication filter: " << std::setw(3)
                  << (done * 100 / total) << "%  " << std::fixed
                  << std::setprecision(2) << elapsed << "s  ("
                  << ImplicationFilterStats::n_comparisons << " cmp, "
                  << ImplicationFilterStats::n_skipped << " skip, "
                  << ImplicationFilterStats::n_duplicates << " dup, "
                  << ImplicationFilterStats::n_timeouts << " timeout)"
                  << std::flush;
    };
    const std::vector<FilterFunction> filters = get_final_filter_functions(
        cfg, original, global_sat_checker(), on_impl_progress);
    const std::vector<Specification> result =
        filter_population(realizable_vec, filters);
    // A final filter drops repairs from the output, so it owes the same
    // accounting as a per-generation one; the report is built from the
    // per-generation list alone and would otherwise not mention these.
    std::vector<FilterRunStats> stats;
    stats.reserve(filters.size());
    for (const FilterFunction& flt : filters) {
        stats.push_back({"final/" + flt.name(), flt.n_in(), flt.n_out()});
    }
    if (cfg.run_implication_filter) {
        const double impl_elapsed =
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          impl_start)
                .count();
        if (stdout_is_tty()) {
            std::cout << "\r\033[K";
        }
        std::cout << "Implication filter: 100%  " << std::fixed
                  << std::setprecision(2) << impl_elapsed << "s  ("
                  << ImplicationFilterStats::n_comparisons << " cmp, "
                  << ImplicationFilterStats::n_skipped << " skip, "
                  << ImplicationFilterStats::n_duplicates << " dup, "
                  << ImplicationFilterStats::n_timeouts << " timeout)\n";
    }
    return {result, std::move(stats)};
}

void write_specifications(
    const std::vector<ScoredSpecification>& scored,
    const AggregateWeightedFitnessFunction& fitness_function,
    const std::string& output_dir) {
    for (std::size_t i = 0; i < scored.size(); ++i) {
        const std::string path =
            output_dir + "/repair_" + std::to_string(i) + ".json";
        std::ofstream file(path);
        if (!file) {
            throw std::runtime_error("cannot open output file: " + path);
        }
        serialisation::FitnessRecord record;
        record.total = scored[i].fitness;
        for (const WeightedFitnessFunction& wff : fitness_function) {
            record.components.push_back(
                {wff.name, wff.function(scored[i].specification), wff.weight});
        }
        const serialisation::ScoredSpecification ssc{scored[i].specification,
                                                     record};
        nlohmann::json jobj = ssc;
        file << jobj.dump(2) << "\n";
    }
}
