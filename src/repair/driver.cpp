#include "driver.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "crash/crash_handler.hpp"
#include "dashboard.hpp"
#include "evolution.hpp"
#include "fitness/function.hpp"
#include "genetic/generation.hpp"
#include "genetic/random_source.hpp"
#include "reports.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "serialisation.hpp"
#include "tlsf/pipeline.hpp"

namespace {

std::string format_crash_metadata(std::size_t seed,
                                  const std::string& input_path,
                                  const Config& cfg) {
    std::ostringstream out;
    out << "Input:            " << input_path << "\n";
    out << "Config:\n";
    out << "  Seed:           " << seed << "\n";
    out << "  Generations:    " << cfg.generations << "\n";
    out << "  Population:     " << cfg.population_size << "\n";
    out << "  Crossover rate: " << cfg.crossover_rate << "\n";
    out << "  Mutation rate:  " << cfg.mutation_rate << "\n";
    out << "  p_trigger:      " << cfg.p_trigger << "\n";
    out << "  p_response:     " << cfg.p_response << "\n";
    out << "  p_timing:       " << cfg.p_timing << "\n";
    out << "  Weight syn:     " << cfg.fitness_weight_syntactic << "\n";
    out << "  Weight sem:     " << cfg.fitness_weight_semantic << "\n";
    out << "  Weight halstead:" << cfg.fitness_weight_halstead << "\n";
    out << "  Weight status:  " << cfg.fitness_weight_status;
    return out.str();
}

RandomSource init_random_source(const std::optional<std::string>& seed_arg) {
    std::random_device rng_dev;
    const std::size_t seed =
        seed_arg.has_value() ? static_cast<std::size_t>(std::stoull(*seed_arg))
                             : static_cast<std::size_t>(rng_dev());
    return make_random_source_from_seed(seed);
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                         start)
        .count();
}

}  // namespace

int run_tlsf_repair(const Config& cfg, const std::string& input_path,
                    const std::string& output_dir,
                    const std::optional<std::string>& seed_arg) {
    const auto wall_start = std::chrono::steady_clock::now();
    RandomSource random_source = init_random_source(seed_arg);
    const std::optional<std::size_t> seed = random_source.seed();
    if (seed.has_value()) {
        register_crash_metadata(format_crash_metadata(*seed, input_path, cfg));
    }
    try {
        const int result =
            tlsf::run_repair(input_path, output_dir, cfg, random_source);
        print_scoring_report();
        print_timing_report();
        if (cfg.report_cpu_timing) {
            print_cpu_report(seconds_since(wall_start));
        }
        return result;
    } catch (const std::exception& exc) {
        std::cerr << "fatal: " << exc.what() << "\n";
        return 1;
    }
}

int run_fretish_repair(const Config& cfg, const std::string& input_path,
                       const std::string& output_dir,
                       const std::optional<std::string>& seed_arg) {
    Specification original_spec;
    try {
        original_spec = load_specification(input_path);
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
    std::cout << "Original specification:\n"
              << strip_atom_prefix(original_spec).to_string() << "\n";
    AggregateWeightedFitnessFunction fitness_function =
        get_fitness_function(original_spec, cfg);
    const std::vector<FilterFunction> filter_functions =
        get_filter_functions(cfg, original_spec, global_sat_checker());
    std::vector<ScoredSpecification> population = original_population(
        original_spec, fitness_function, cfg.population_size);
    RandomSource random_source = init_random_source(seed_arg);
    const std::optional<std::size_t> maybe_seed = random_source.seed();
    if (!maybe_seed.has_value()) {
        std::cerr << "fatal: random source has no seed\n";
        return 1;
    }
    const std::size_t seed = *maybe_seed;
    std::cout << "Seed: " << seed << "\n";
    register_crash_metadata(format_crash_metadata(seed, input_path, cfg));

    DashboardWriter dashboard(output_dir, cfg.dashboard);
    dashboard.run_start(input_path, cfg.generations, population.size(), seed,
                        fitness_objective_names(fitness_function),
                        generation_stage_names(filter_functions));
    const std::string page = dashboard.write_page();
    if (!page.empty()) {
        std::cout << "Progress: " << dashboard.path() << "\n"
                  << "  view with: python3 -m http.server -d " << output_dir
                  << " 8000\n";
    }

    const auto wall_start = std::chrono::steady_clock::now();
    try {
        auto [population_result, filter_stats] =
            run_evolution(cfg, std::move(population), fitness_function,
                          filter_functions, random_source, dashboard);
        population = std::move(population_result);
        const std::vector<Specification> realizable_vec =
            collect_realizable_specifications(population);
        auto [maximal, final_filter_stats] =
            filter_maximal_specifications(cfg, original_spec, realizable_vec);
        const std::vector<ScoredSpecification> scored_maximal =
            score_and_sort_specifications(cfg, maximal, fitness_function);
        write_specifications(scored_maximal, fitness_function, output_dir);
        std::cout << "Realizable specifications: " << realizable_vec.size();
        if (cfg.run_implication_filter) {
            std::cout << " (" << maximal.size() << " maximal)";
        }
        std::cout << ", written to " << output_dir << "/\n";
        dashboard.run_end(cfg.generations, realizable_vec.size(),
                          maximal.size(), seconds_since(wall_start));
        filter_stats.insert(filter_stats.end(), final_filter_stats.begin(),
                            final_filter_stats.end());
        print_filter_report(filter_stats);
        print_scoring_report();
        print_timing_report();
        if (cfg.report_cpu_timing) {
            print_cpu_report(seconds_since(wall_start));
        }
    } catch (const std::exception& exc) {
        std::cerr << "fatal: " << exc.what() << "\n";
        return 1;
    }
    return 0;
}
