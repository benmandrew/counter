#include "tlsf/pipeline.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "genetic/generation.hpp"
#include "genetic/random_source.hpp"
#include "genetic/scored.hpp"
#include "runner/black.hpp"
#include "serialisation.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/operators.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"
#include "tlsf/writer.hpp"

namespace tlsf {

namespace {

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// Realizable survivors of the final population, deduplicated by value while
// preserving fitness order.
std::vector<Scored<Specification>> realizable_survivors(
    const std::vector<Scored<Specification>>& population, const Config& cfg,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness) {
    std::vector<Scored<Specification>> survivors;
    for (const Scored<Specification>& scored : population) {
        if (tlsf_status(scored.specification, cfg) != 1.0) {
            continue;
        }
        const bool seen =
            std::any_of(survivors.begin(), survivors.end(),
                        [&scored](const Scored<Specification>& kept) {
                            return kept.specification == scored.specification;
                        });
        if (!seen) {
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

// Applies the implication (maximality) filter to the survivor specifications
// and returns the Scored entries whose spec survived, in the original order.
std::vector<Scored<Specification>> keep_maximal(
    const std::vector<Scored<Specification>>& survivors,
    SatisfiabilityChecker& checker) {
    std::vector<Specification> specs;
    specs.reserve(survivors.size());
    for (const Scored<Specification>& scored : survivors) {
        specs.push_back(scored.specification);
    }
    const std::vector<Specification> maximal =
        tlsf_make_implication_filter(checker)(specs);
    std::vector<Scored<Specification>> result;
    result.reserve(maximal.size());
    for (const Scored<Specification>& scored : survivors) {
        const bool kept = std::any_of(maximal.begin(), maximal.end(),
                                      [&scored](const Specification& spec) {
                                          return spec == scored.specification;
                                      });
        if (kept) {
            result.push_back(scored);
        }
    }
    return result;
}

void write_survivors(
    const std::vector<Scored<Specification>>& survivors,
    const AggregateWeightedFitnessFunctionT<Specification>& fitness,
    const std::string& output_dir) {
    for (std::size_t i = 0; i < survivors.size(); ++i) {
        const std::string base = output_dir + "/repair_" + std::to_string(i);
        std::ofstream spec_file(base + ".tlsf");
        if (!spec_file) {
            throw std::runtime_error("cannot open output file: " + base +
                                     ".tlsf");
        }
        spec_file << write(survivors[i].specification);

        std::ofstream fitness_file(base + ".fitness.json");
        if (!fitness_file) {
            throw std::runtime_error("cannot open output file: " + base +
                                     ".fitness.json");
        }
        // Mirror the FRETISH per-objective breakdown: the weighted total plus
        // each component's score and weight, in registration order.
        serialisation::FitnessRecord record;
        record.total = survivors[i].fitness;
        for (const WeightedFitnessFunctionT<Specification>& wff : fitness) {
            record.components.push_back(
                {wff.name, wff.function(survivors[i].specification),
                 wff.weight});
        }
        const nlohmann::json jobj = record;
        fitness_file << jobj.dump(2) << "\n";
    }
}

struct FilterRunStats {
    std::string name;
    std::size_t total_in = 0;
    std::size_t total_out = 0;
};

void print_filter_report(const std::vector<FilterRunStats>& stats) {
    const bool any =
        std::any_of(stats.begin(), stats.end(), [](const FilterRunStats& stat) {
            return !stat.name.empty() && stat.total_in > 0;
        });
    if (!any) {
        return;
    }
    std::cout << "\nFilter report:\n";
    for (const FilterRunStats& stat : stats) {
        if (stat.name.empty() || stat.total_in == 0) {
            continue;
        }
        const double pct_drop =
            100.0 * (1.0 - static_cast<double>(stat.total_out) /
                               static_cast<double>(stat.total_in));
        std::cout << std::left << std::setw(16) << stat.name << std::right
                  << std::setw(8) << stat.total_in << " in  " << std::setw(8)
                  << stat.total_out << " out  " << std::fixed
                  << std::setprecision(1) << std::setw(5) << pct_drop
                  << "% avg drop\n";
    }
}

}  // namespace

int run_repair(const std::string& input_path, const std::string& output_dir,
               const Config& cfg, const RandomSource& random_source) {
    const std::optional<std::string> text = read_file(input_path);
    if (!text.has_value()) {
        std::cerr << "cannot read input file: " << input_path << "\n";
        return 1;
    }
    Specification original;
    try {
        original = parse(*text);
    } catch (const std::invalid_argument& exc) {
        std::cerr << "parse error: " << exc.what() << "\n";
        return 1;
    }

    std::cout << "Original specification (LTL):\n  " << original.to_ltl()
              << "\n";
    const std::optional<std::size_t> maybe_seed = random_source.seed();
    if (maybe_seed.has_value()) {
        std::cout << "Seed: " << *maybe_seed << "\n";
    }

    const AggregateWeightedFitnessFunctionT<Specification> fitness =
        tlsf_get_fitness_function(original, cfg);

    // Per-generation filters, the TLSF counterparts of the FRETISH set
    // (dedup, bloat cap, assumption-satisfiability = false-condition, and the
    // optional weakening filter), each with its configured interval.
    std::vector<FilterFunctionT<Specification>> per_gen_filters;
    {
        FilterFunctionT<Specification> dedup = tlsf_make_dedup_filter();
        dedup.set_interval(cfg.dedup_filter_interval);
        per_gen_filters.push_back(std::move(dedup));
        FilterFunctionT<Specification> bloat =
            tlsf_make_bloat_cap_filter(original);
        bloat.set_interval(cfg.bloat_filter_interval);
        per_gen_filters.push_back(std::move(bloat));
        FilterFunctionT<Specification> assumption_sat =
            tlsf_make_assumption_sat_filter();
        assumption_sat.set_interval(cfg.false_condition_filter_interval);
        per_gen_filters.push_back(std::move(assumption_sat));
        if (cfg.run_weakening_filter) {
            FilterFunctionT<Specification> weakening =
                tlsf_make_weakening_filter(original, global_sat_checker());
            weakening.set_interval(cfg.weakening_filter_interval);
            per_gen_filters.push_back(std::move(weakening));
        }
    }

    const std::vector<Specification> seed_population(cfg.population_size,
                                                     original);
    std::vector<Scored<Specification>> population =
        score_population(cfg, seed_population, fitness);

    // Truncation selection with elitism, matching the FRETISH path: each
    // generation breeds down to selection_size survivors, carrying the best
    // elitism_size over verbatim. Both are derived once from the seed
    // population size (cfg guarantees elitism_rate < selection_rate).
    const std::size_t selection_size = std::max(
        std::size_t{1},
        static_cast<std::size_t>(static_cast<double>(cfg.population_size) *
                                 cfg.selection_rate));
    const auto elitism_size = static_cast<std::size_t>(
        static_cast<double>(cfg.population_size) * cfg.elitism_rate);

    std::vector<FilterRunStats> filter_stats;
    filter_stats.reserve(per_gen_filters.size());
    for (const FilterFunctionT<Specification>& filter : per_gen_filters) {
        filter_stats.push_back({filter.name(), 0, 0});
    }

    for (std::size_t gen = 0; gen < cfg.generations; ++gen) {
        const bool is_last = gen + 1 == cfg.generations;
        std::vector<FilterFunctionT<Specification>> active;
        std::vector<std::size_t> active_index;
        active.reserve(per_gen_filters.size());
        active_index.reserve(per_gen_filters.size());
        for (std::size_t k = 0; k < per_gen_filters.size(); ++k) {
            const FilterFunctionT<Specification>& filter = per_gen_filters[k];
            if (is_last || (gen + 1) % filter.interval() == 0) {
                active.push_back(filter);
                active_index.push_back(k);
            }
        }
        population = evolve_generation_generic(cfg, population, selection_size,
                                               elitism_size, fitness, active,
                                               tlsf_operators(), random_source);
        // The active copies hold this generation's in/out sizes; fold them into
        // the running per-filter totals for the end-of-run report.
        for (std::size_t k = 0; k < active.size(); ++k) {
            filter_stats[active_index[k]].total_in += active[k].n_in();
            filter_stats[active_index[k]].total_out += active[k].n_out();
        }
        const double best =
            population.empty() ? 0.0 : population.front().fitness;
        std::cout << "gen " << (gen + 1) << "/" << cfg.generations
                  << "  best fitness " << best << "\n";
    }

    std::vector<Scored<Specification>> survivors =
        realizable_survivors(population, cfg, fitness);
    const std::size_t n_realizable = survivors.size();
    // Final maximality pass: keep only realizable repairs not strictly implied
    // by another, mirroring the FRETISH final implication filter.
    if (cfg.run_implication_filter && survivors.size() > 1) {
        survivors = keep_maximal(survivors, global_sat_checker());
    }
    write_survivors(survivors, fitness, output_dir);
    std::cout << "Realizable specifications: " << n_realizable;
    if (cfg.run_implication_filter) {
        std::cout << " (" << survivors.size() << " maximal)";
    }
    if (!survivors.empty()) {
        std::cout << ", written to " << output_dir << "/";
    }
    std::cout << "\n";
    print_filter_report(filter_stats);
    return 0;
}

}  // namespace tlsf
