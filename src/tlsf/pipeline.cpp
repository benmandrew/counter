#include "tlsf/pipeline.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
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
            survivors.push_back(
                {scored.specification, fitness(scored.specification)});
        }
    }
    std::sort(
        survivors.begin(), survivors.end(),
        [](const Scored<Specification>& lhs, const Scored<Specification>& rhs) {
            return lhs.fitness > rhs.fitness;
        });
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

void write_survivors(const std::vector<Scored<Specification>>& survivors,
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
        nlohmann::json record;
        record["fitness"] = survivors[i].fitness;
        fitness_file << record.dump(2) << "\n";
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

    for (std::size_t gen = 0; gen < cfg.generations; ++gen) {
        const bool is_last = gen + 1 == cfg.generations;
        std::vector<FilterFunctionT<Specification>> active;
        active.reserve(per_gen_filters.size());
        for (const FilterFunctionT<Specification>& filter : per_gen_filters) {
            if (is_last || (gen + 1) % filter.interval() == 0) {
                active.push_back(filter);
            }
        }
        population = evolve_generation_generic(
            cfg, population, cfg.population_size, fitness, active,
            tlsf_operators(), random_source);
        const double best =
            population.empty() ? 0.0 : population.front().fitness;
        std::cout << "gen " << (gen + 1) << "/" << cfg.generations
                  << "  best fitness " << best << "\n";
    }

    std::vector<Scored<Specification>> survivors =
        realizable_survivors(population, cfg, fitness);
    // Final maximality pass: keep only realizable repairs not strictly implied
    // by another, mirroring the FRETISH final implication filter.
    if (cfg.run_implication_filter && survivors.size() > 1) {
        survivors = keep_maximal(survivors, global_sat_checker());
    }
    write_survivors(survivors, output_dir);
    std::cout << "Realizable specifications: " << survivors.size();
    if (!survivors.empty()) {
        std::cout << ", written to " << output_dir << "/";
    }
    std::cout << "\n";
    return 0;
}

}  // namespace tlsf
