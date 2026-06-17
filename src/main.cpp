#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "config.hpp"
#include "crash/crash_handler.hpp"
#include "filter/implication.hpp"
#include "fitness/function.hpp"
#include "fitness/status.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/ganak.hpp"
#include "runner/spot.hpp"
#include "serialisation.hpp"

std::optional<std::string> parse_string_arg(int argc, const char* const* argv,
                                            const char* flag) {
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            if (argv[i + 1] != nullptr) {
                return std::string(argv[i + 1]);
            }
        }
    }
    return std::nullopt;
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

struct FilterRunStats {
    std::string name;
    std::size_t total_in{0};
    std::size_t total_out{0};
};

void print_filter_report(const std::vector<FilterRunStats>& stats) {
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

void print_timing_report() {
    auto print_row = [](const char* name, std::size_t calls, double total_s,
                        std::size_t cache_hits, std::size_t timeouts = 0) {
        const double avg_s =
            calls > 0 ? total_s / static_cast<double>(calls) : 0.0;
        std::cout << std::left << std::setw(12) << name << std::right
                  << std::setw(6) << calls << " calls  " << std::fixed
                  << std::setprecision(3) << std::setw(8) << total_s
                  << "s total  " << std::setw(8) << avg_s << "s avg";
        if (cache_hits > 0) {
            std::cout << "  (+" << cache_hits << " cache hits)";
        }
        if (timeouts > 0) {
            std::cout << "  (" << timeouts << " timeouts)";
        }
        std::cout << "\n";
    };
    std::cout << "\nTool timing report:\n";
    print_row("ltl2tgba", Ltl2tgbaStats::n_cache_misses,
              Ltl2tgbaStats::total_time_s, Ltl2tgbaStats::n_cache_hits);
    print_row("ltlsynt", RealizabilityChecker::n_cache_misses,
              RealizabilityChecker::total_time_s,
              RealizabilityChecker::n_cache_hits);
    print_row("black", SatisfiabilityChecker::n_cache_misses,
              SatisfiabilityChecker::total_time_s,
              SatisfiabilityChecker::n_cache_hits,
              SatisfiabilityChecker::n_timeouts);
    print_row("ganak", GanakStats::n_cache_misses, GanakStats::total_time_s,
              GanakStats::n_cache_hits);
    std::cout << "\nFitness cache: "
              << AggregateWeightedFitnessFunction::n_cache_hits << " hits / "
              << AggregateWeightedFitnessFunction::n_cache_misses
              << " misses\n";
}

std::vector<ScoredSpecification> original_population(
    Specification& original_spec,
    const AggregateWeightedFitnessFunction& fitness_function,
    std::size_t population_size) {
    std::vector<ScoredSpecification> population;
    population.reserve(population_size);
    for (std::size_t i = 0; i < population_size; ++i) {
        population.push_back({original_spec, fitness_function(original_spec)});
    }
    return population;
}

std::optional<std::size_t> parse_seed_arg(int argc, const char* const* argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == "--seed") {
            if (argv[i + 1] != nullptr) {
                return static_cast<std::size_t>(std::stoull(argv[i + 1]));
            }
        }
    }
    return std::nullopt;
}

std::string format_crash_metadata(std::size_t seed,
                                  const std::string& input_path) {
    std::ostringstream out;
    out << "Input:            " << input_path << "\n";
    out << "Config:\n";
    out << "  Seed:           " << seed << "\n";
    out << "  Generations:    " << Config::generations << "\n";
    out << "  Population:     " << Config::population_size << "\n";
    out << "  Crossover rate: " << Config::crossover_rate << "\n";
    out << "  Mutation rate:  " << Config::mutation_rate << "\n";
    out << "  p_trigger:      " << Config::p_trigger << "\n";
    out << "  p_response:     " << Config::p_response << "\n";
    out << "  p_timing:       " << Config::p_timing << "\n";
    out << "  Weight syn:     " << Config::fitness_weight_syntactic << "\n";
    out << "  Weight sem:     " << Config::fitness_weight_semantic << "\n";
    out << "  Weight halstead:" << Config::fitness_weight_halstead << "\n";
    out << "  Weight status:  " << Config::fitness_weight_status;
    return out.str();
}

RandomSource init_random_source(int argc, const char* const* argv) {
    const std::optional<std::size_t> seed_arg = parse_seed_arg(argc, argv);
    std::random_device rng_dev;
    const std::size_t seed =
        seed_arg.has_value() ? *seed_arg : static_cast<std::size_t>(rng_dev());
    return make_random_source_from_seed(seed);
}

std::pair<std::vector<ScoredSpecification>, std::vector<FilterRunStats>>
run_evolution(std::vector<ScoredSpecification> population,
              const AggregateWeightedFitnessFunction& fitness_function,
              const std::vector<FilterFunction>& filter_functions,
              RandomSource& random_source) {
    std::vector<FilterRunStats> filter_stats;
    filter_stats.reserve(filter_functions.size());
    for (const FilterFunction& flt : filter_functions) {
        filter_stats.push_back({flt.name(), 0, 0});
    }
    const std::size_t pop_size = population.size();
    for (std::size_t gen_idx = 0; gen_idx < Config::generations; ++gen_idx) {
        const auto start = std::chrono::steady_clock::now();
        auto on_progress = [&](std::size_t done, std::size_t total) {
            const double elapsed = std::chrono::duration<double>(
                                       std::chrono::steady_clock::now() - start)
                                       .count();
            std::cout << "\rGeneration " << std::setw(2) << gen_idx + 1 << ": "
                      << std::setw(3) << (done * 100 / total) << "%  "
                      << std::fixed << std::setprecision(2) << elapsed << "s"
                      << std::flush;
        };
        population =
            evolve_generation(population, pop_size, fitness_function,
                              filter_functions, random_source, on_progress);
        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        std::cout << "\r\033[KGeneration " << std::setw(2) << gen_idx + 1
                  << ": 100%  " << std::fixed << std::setprecision(2) << elapsed
                  << "s\n";
        for (std::size_t i = 0; i < filter_functions.size(); ++i) {
            filter_stats[i].total_in += filter_functions[i].n_in();
            filter_stats[i].total_out += filter_functions[i].n_out();
        }
    }
    return {std::move(population), std::move(filter_stats)};
}

std::vector<Specification> collect_realizable_specifications(
    const std::vector<ScoredSpecification>& population) {
    std::vector<Specification> realizable_vec;
    for (const ScoredSpecification& scored : population) {
        // The per-generation filter only screens offspring during evolution,
        // so a false-triggered result from the final generation would
        // otherwise never be re-screened before being reported here.
        if (specification_status(scored.specification, global_sat_checker(),
                                 global_real_checker()) == 1.0 &&
            !specification_has_false_trigger(scored.specification)) {
            realizable_vec.push_back(scored.specification);
        }
    }
    return realizable_vec;
}

// Reduces realizable specifications to those maximal under implication, when
// Config::run_implication_filter is enabled; otherwise returns them unchanged.
std::vector<Specification> filter_maximal_specifications(
    const std::vector<Specification>& realizable_vec) {
    if (!Config::run_implication_filter) {
        return realizable_vec;
    }
    const auto impl_start = std::chrono::steady_clock::now();
    auto on_impl_progress = [&](std::size_t done, std::size_t total) {
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
    const FilterFunction implication_filter =
        make_implication_filter(global_sat_checker(), on_impl_progress);
    std::vector<Specification> maximal = implication_filter(realizable_vec);
    const double impl_elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                      impl_start)
            .count();
    std::cout << "\r\033[KImplication filter: 100%  " << std::fixed
              << std::setprecision(2) << impl_elapsed << "s  ("
              << ImplicationFilterStats::n_comparisons << " cmp, "
              << ImplicationFilterStats::n_skipped << " skip, "
              << ImplicationFilterStats::n_duplicates << " dup, "
              << ImplicationFilterStats::n_timeouts << " timeout)\n";
    return maximal;
}

bool has_flag(int argc, const char* const* argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] != nullptr && std::string(argv[i]) == flag) {
            return true;
        }
    }
    return false;
}

void print_help(const char* prog) {
    std::cout
        << "Usage: " << prog
        << " --input <spec.json> --output-dir <dir> [--seed <n>]\n"
        << "\n"
        << "Repair an unrealisable FRETISH specification using a genetic\n"
        << "algorithm. The input specification is read from a JSON file and\n"
        << "the algorithm evolves a population of candidate repairs, scoring\n"
        << "each by syntactic similarity, semantic similarity, Halstead\n"
        << "complexity, and LTL realisability.\n"
        << "\n"
        << "Options:\n"
        << "  --input <spec.json>  Path to the input specification "
           "(required).\n"
        << "                       Accepts plain Specification JSON or a\n"
        << "                       ScoredSpecification with an optional\n"
        << "                       \"fitness\" field.\n"
        << "  --output-dir <dir>   Directory to write maximal realizable\n"
        << "                       repairs to as repair_0.json, "
           "repair_1.json,\n"
        << "                       ... (required; directory must already "
           "exist).\n"
        << "  --seed <n>           RNG seed for reproducible runs. If omitted\n"
        << "                       a random seed is chosen and printed.\n"
        << "  -h, --help           Show this help message and exit.\n"
        << "\n"
        << "Input format (examples/takeoff.json):\n"
        << "  {\n"
        << "    \"assumptions\": [],\n"
        << "    \"guarantees\":  [ { \"trigger\": \"<formula>\",\n"
        << "                        \"response\": \"<formula>\",\n"
        << "                        \"timing\":   { \"type\": \"Immediately\" "
           "} } ],\n"
        << "    \"in_atoms\":  [\"a\", \"b\"],\n"
        << "    \"out_atoms\": [\"x\", \"y\"]\n"
        << "  }\n"
        << "\n"
        << "Timing types: Immediately, NextTimepoint, Eventually,\n"
        << "              WithinTicks {\"ticks\": n}, ForTicks {\"ticks\": "
           "n},\n"
        << "              AfterTicks  {\"ticks\": n}\n";
}

int main(int argc, const char* const argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    init_cpptrace(argv[0]);
    if (has_flag(argc, argv, "-h") || has_flag(argc, argv, "--help")) {
        print_help(argv[0]);
        return 0;
    }
    const std::optional<std::string> input_path =
        parse_string_arg(argc, argv, "--input");
    const std::optional<std::string> output_dir =
        parse_string_arg(argc, argv, "--output-dir");
    if (!input_path.has_value() || !output_dir.has_value()) {
        std::cerr << "Usage: " << argv[0]
                  << " --input <spec.json> --output-dir <dir> [--seed <n>]\n"
                  << "Try '" << argv[0] << " --help' for more information.\n";
        return 1;
    }
    if (!std::filesystem::is_directory(*output_dir)) {
        std::cerr << "Output directory does not exist: " << *output_dir << "\n";
        return 1;
    }
    Specification original_spec;
    try {
        original_spec = load_specification(*input_path);
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
    std::cout << "Original specification:\n"
              << original_spec.to_string() << "\n";
    AggregateWeightedFitnessFunction fitness_function =
        get_fitness_function(original_spec);
    const std::vector<FilterFunction> filter_functions =
        get_filter_functions(original_spec, global_sat_checker());
    std::vector<ScoredSpecification> population = original_population(
        original_spec, fitness_function, Config::population_size);
    RandomSource random_source = init_random_source(argc, argv);
    const std::optional<std::size_t> maybe_seed = random_source.seed();
    if (!maybe_seed.has_value()) {
        std::cerr << "fatal: random source has no seed\n";
        return 1;
    }
    const std::size_t seed = *maybe_seed;
    std::cout << "Seed: " << seed << "\n";
    register_crash_metadata(format_crash_metadata(seed, *input_path));
    auto [population_result, filter_stats] =
        run_evolution(std::move(population), fitness_function, filter_functions,
                      random_source);
    population = std::move(population_result);
    const std::vector<Specification> realizable_vec =
        collect_realizable_specifications(population);
    const std::vector<Specification> maximal =
        filter_maximal_specifications(realizable_vec);
    const std::vector<ScoredSpecification> scored_maximal =
        score_and_sort_specifications(maximal, fitness_function);
    try {
        write_specifications(scored_maximal, fitness_function, *output_dir);
    } catch (const std::exception& exc) {
        std::cerr << exc.what() << "\n";
        return 1;
    }
    std::cout << "Realizable specifications: " << realizable_vec.size();
    if (Config::run_implication_filter) {
        std::cout << " (" << maximal.size() << " maximal)";
    }
    std::cout << ", written to " << *output_dir << "/\n";
    print_filter_report(filter_stats);
    print_timing_report();
    return 0;
}
