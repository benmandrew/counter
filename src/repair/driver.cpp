#include "driver.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <iomanip>
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
#include "filter/correctness.hpp"
#include "fitness/function.hpp"
#include "genetic/generation.hpp"
#include "genetic/random_source.hpp"
#include "manifest.hpp"
#include "profile.hpp"
#include "reports.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"
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

RandomSource init_random_source(const std::optional<std::size_t>& seed) {
    if (seed.has_value()) {
        return make_random_source_from_seed(*seed);
    }
    std::random_device rng_dev;
    return make_random_source_from_seed(static_cast<std::size_t>(rng_dev()));
}

double seconds_since(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                         start)
        .count();
}

}  // namespace

int run_tlsf_repair(const Config& cfg, const std::string& input_path,
                    const std::string& output_dir,
                    const std::optional<std::size_t>& seed) {
    const auto wall_start = std::chrono::steady_clock::now();
    RandomSource random_source = init_random_source(seed);
    const std::optional<std::size_t> effective_seed = random_source.seed();
    if (effective_seed.has_value()) {
        register_crash_metadata(
            format_crash_metadata(*effective_seed, input_path, cfg));
    }
    try {
        const int result =
            tlsf::run_repair(input_path, output_dir, cfg, random_source);
        print_scoring_report();
        if (cfg.report_diagnostics) {
            print_diagnostics_report();
        }
        if (cfg.report_cpu_timing) {
            print_cpu_report(seconds_since(wall_start));
        }
        // The per-tool rows say how long each tool took; the scope profile says
        // where inside a call that went. No-op unless COUNTER_PROFILE is set,
        // so it is not tied to --diagnostics.
        profile::report_if_enabled();
        // After the reports, so the per-tool counts it records are the run's
        // totals. Skipped when run_repair failed to read or parse the input,
        // since there is then no output directory worth describing.
        if (result == 0 && effective_seed.has_value()) {
            write_run_manifest(output_dir, input_path, *effective_seed, cfg,
                               seconds_since(wall_start));
        }
        // Last on both paths, so the figure covers the same work either way.
        std::cout << "Done in " << std::fixed << std::setprecision(2)
                  << seconds_since(wall_start) << "s\n";
        return result;
    } catch (const std::exception& exc) {
        std::cerr << "fatal: " << exc.what() << "\n";
        return 1;
    }
}

int run_fretish_repair(const Config& cfg, const std::string& input_path,
                       const std::string& output_dir,
                       const std::optional<std::size_t>& seed) {
    Specification original_spec;
    try {
        original_spec = load_specification(input_path);
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
    // Screened before anything is built from it. The seed population is
    // population_size copies of this specification and no filter ever sees
    // them, so a property the gate will reject at the output is worth saying
    // out loud at the start of the run rather than at the end of it.
    if (const std::optional<std::string> failed = first_failing_check(
            original_spec,
            correctness_checks(global_sat_checker(), global_real_checker()));
        failed.has_value()) {
        InputScreen::failed_check = *failed;
        std::cerr << input_screen_warning(*failed);
    }
    // After the input screen and before anything is scored: the query is
    // memoised, so this is the call the original population was about to make
    // anyway, timed.
    std::cerr << ltlsynt_budget_screen(
        [&original_spec] {
            return global_real_checker().check_realizability(original_spec);
        },
        cfg.ltlsynt_timeout);
    AggregateWeightedFitnessFunction fitness_function =
        get_fitness_function(original_spec, cfg);
    const std::vector<FilterFunction> filter_functions =
        get_filter_functions(cfg, original_spec, global_sat_checker());
    std::vector<ScoredSpecification> population = original_population(
        original_spec, fitness_function, cfg.population_size);
    RandomSource random_source = init_random_source(seed);
    const std::optional<std::size_t> maybe_seed = random_source.seed();
    if (!maybe_seed.has_value()) {
        std::cerr << "fatal: random source has no seed\n";
        return 1;
    }
    const std::size_t effective_seed = *maybe_seed;
    std::cout << "Seed: " << effective_seed << "\n";
    register_crash_metadata(
        format_crash_metadata(effective_seed, input_path, cfg));

    DashboardWriter dashboard(output_dir, cfg.dashboard);
    dashboard.run_start(input_path, cfg.generations, population.size(),
                        effective_seed,
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
        // Only when something was written: the line otherwise names an output
        // directory that the run left empty, which reads as success.
        if (!scored_maximal.empty()) {
            std::cout << ", written to " << output_dir << "/";
        }
        std::cout << "\n";
        dashboard.run_end(cfg.generations, realizable_vec.size(),
                          maximal.size(), seconds_since(wall_start));
        filter_stats.insert(filter_stats.end(), final_filter_stats.begin(),
                            final_filter_stats.end());
        print_filter_report(filter_stats);
        print_scoring_report();
        if (cfg.report_diagnostics) {
            print_diagnostics_report();
        }
        if (cfg.report_cpu_timing) {
            print_cpu_report(seconds_since(wall_start));
        }
        profile::report_if_enabled();
        // After the reports, so the per-tool counts it records are the run's
        // totals.
        write_run_manifest(output_dir, input_path, effective_seed, cfg,
                           seconds_since(wall_start));
        std::cout << "Done in " << std::fixed << std::setprecision(2)
                  << seconds_since(wall_start) << "s\n";
    } catch (const std::exception& exc) {
        std::cerr << "fatal: " << exc.what() << "\n";
        return 1;
    }
    return 0;
}
