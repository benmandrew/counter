#include <sys/resource.h>

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
#include "config_io.hpp"
#include "crash/crash_handler.hpp"
#include "filter/implication.hpp"
#include "filter/vacuity.hpp"
#include "fitness/function.hpp"
#include "fitness/status.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/ganak.hpp"
#include "runner/ltlfilt.hpp"
#include "runner/spot.hpp"
#include "serialisation.hpp"
#include "status_line.hpp"
#include "tlsf/pipeline.hpp"

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

// Silent when nothing was dropped, so a clean run's output is unchanged and
// any drop at all stands out.
void print_scoring_report() {
    if (ScoringStats::n_dropped == 0) {
        return;
    }
    std::cout << "\nScoring report:\n"
              << ScoringStats::n_dropped
              << " individual(s) dropped after a fitness function threw. These "
                 "were excluded from their generation rather than scored.\n";
    for (const auto& [reason, count] : ScoringStats::reasons) {
        std::cout << "  " << std::setw(6) << count << "x  " << reason << "\n";
    }
    if (ScoringStats::n_reasons_elided > 0) {
        std::cout << "  (" << ScoringStats::n_reasons_elided
                  << " further failure(s) with other messages not listed)\n";
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
    print_row("ltlfilt", LtlfiltStats::n_cache_misses,
              LtlfiltStats::total_time_s, LtlfiltStats::n_cache_hits);
    print_row("ltlsynt", RealizabilityChecker::n_cache_misses,
              RealizabilityChecker::total_time_s,
              RealizabilityChecker::n_cache_hits,
              RealizabilityChecker::n_timeouts);
    print_row("black", SatisfiabilityChecker::n_cache_misses,
              SatisfiabilityChecker::total_time_s,
              SatisfiabilityChecker::n_cache_hits,
              SatisfiabilityChecker::n_timeouts);
    print_row("ganak", GanakStats::n_cache_misses, GanakStats::total_time_s,
              GanakStats::n_cache_hits);
    if (Ltl2tgbaStats::n_tautology_substitutions > 0) {
        std::cout << "\nltl2tgba tautology substitutions (SPOT exit-2 bug, "
                     "treated as trivially true): "
                  << Ltl2tgbaStats::n_tautology_substitutions << "\n";
    }
    std::cout << "\nConstant-folded (decided by ltlfilt, no black call): "
              << SatisfiabilityChecker::n_constant_folded << "\n";
    std::cout << "\nFitness cache: "
              << AggregateWeightedFitnessFunction::n_cache_hits << " hits / "
              << AggregateWeightedFitnessFunction::n_cache_misses
              << " misses\n";
}

// Reports where CPU actually went: this process's own code (all threads) vs.
// the external CLI tools (separate child processes). getrusage gives the
// authoritative self/children split; the per-tool rows are attributed from
// each wrapper's wait4() and should sum to roughly the children total (the
// remainder is uninstrumented children, e.g. ltlfilt's equivalence check).
void print_cpu_report(double wall_s) {
    auto secs = [](const timeval& tval) {
        return static_cast<double>(tval.tv_sec) +
               (static_cast<double>(tval.tv_usec) / 1e6);
    };
    struct rusage self_ru{};
    struct rusage child_ru{};
    getrusage(RUSAGE_SELF, &self_ru);
    getrusage(RUSAGE_CHILDREN, &child_ru);
    const double self_cpu = secs(self_ru.ru_utime) + secs(self_ru.ru_stime);
    const double child_cpu = secs(child_ru.ru_utime) + secs(child_ru.ru_stime);
    const double total_cpu = self_cpu + child_cpu;

    auto pct = [total_cpu](double part) {
        return total_cpu > 0.0 ? 100.0 * part / total_cpu : 0.0;
    };
    auto line = [&pct](const char* name, double cpu_s) {
        std::cout << std::left << std::setw(20) << name << std::right
                  << std::fixed << std::setprecision(3) << std::setw(9) << cpu_s
                  << "s cpu  " << std::setprecision(1) << std::setw(5)
                  << pct(cpu_s) << "%\n";
    };

    std::cout << "\nCPU attribution (wall " << std::fixed
              << std::setprecision(3) << wall_s << "s, total cpu " << total_cpu
              << "s):\n";
    line("your code", self_cpu);
    line("CLI tools (total)", child_cpu);
    std::cout << "  per tool:\n";
    line("  ltl2tgba", Ltl2tgbaStats::total_cpu_s);
    line("  ltlfilt", LtlfiltStats::total_cpu_s);
    line("  ltlsynt", RealizabilityChecker::total_cpu_s);
    line("  black", SatisfiabilityChecker::total_cpu_s);
    line("  ganak", GanakStats::total_cpu_s);
    const double attributed =
        Ltl2tgbaStats::total_cpu_s + LtlfiltStats::total_cpu_s +
        RealizabilityChecker::total_cpu_s + SatisfiabilityChecker::total_cpu_s +
        GanakStats::total_cpu_s;
    line("  unattributed", child_cpu - attributed);
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

RandomSource init_random_source(int argc, const char* const* argv) {
    const std::optional<std::size_t> seed_arg = parse_seed_arg(argc, argv);
    std::random_device rng_dev;
    const std::size_t seed =
        seed_arg.has_value() ? *seed_arg : static_cast<std::size_t>(rng_dev());
    return make_random_source_from_seed(seed);
}

std::pair<std::vector<ScoredSpecification>, std::vector<FilterRunStats>>
run_evolution(const Config& cfg, std::vector<ScoredSpecification> population,
              const AggregateWeightedFitnessFunction& fitness_function,
              const std::vector<FilterFunction>& filter_functions,
              RandomSource& random_source) {
    std::vector<FilterRunStats> filter_stats;
    filter_stats.reserve(filter_functions.size());
    for (const FilterFunction& flt : filter_functions) {
        filter_stats.push_back({flt.name(), 0, 0});
    }
    StatusLine status;
    const std::size_t col_gen = status.add("gen");
    const std::size_t col_pct = status.add("%");
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

        // Each per-generation filter runs only every interval()-th generation;
        // the final generation always runs every filter, so the returned
        // population is never left unfiltered.
        const std::vector<FilterFunction> active_filters =
            filters_for_generation(filter_functions, gen_idx + 1,
                                   gen_idx + 1 == cfg.generations);

        population = evolve_generation(
            cfg, population, selection_size, elitism_size, fitness_function,
            active_filters, random_source, on_progress);

        // Each active filter copy carries this generation's in/out sizes; fold
        // them into the running per-filter totals reported at the end.
        for (const FilterFunction& flt : active_filters) {
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
        status.set(col_pct, "100%");
        status.set(col_time, format_elapsed(elapsed));
        if (!population.empty()) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(3) << population[0].fitness;
            status.set(col_best, oss.str());
        }
        std::size_t n_real = 0;
        for (const ScoredSpecification& cand : population) {
            if (specification_status(cand.specification, global_sat_checker(),
                                     global_real_checker()) == 1.0 &&
                !specification_has_false_condition(cand.specification) &&
                !specification_has_unsatisfiable_assumptions(
                    cand.specification, global_sat_checker())) {
                ++n_real;
            }
        }
        status.set(col_real, std::to_string(n_real));
        status.render();
        status.finish();
    }
    return {std::move(population), std::move(filter_stats)};
}

std::vector<Specification> collect_realizable_specifications(
    const std::vector<ScoredSpecification>& population) {
    std::vector<Specification> realizable_vec;
    for (const ScoredSpecification& scored : population) {
        // The per-generation filter only screens offspring during evolution,
        // so a false-condition result from the final generation would
        // otherwise never be re-screened before being reported here. The same
        // applies to vacuously-realizable candidates: elites bypass the
        // offspring filters entirely, so one can reach the output unscreened.
        if (specification_status(scored.specification, global_sat_checker(),
                                 global_real_checker()) == 1.0 &&
            !specification_has_false_condition(scored.specification) &&
            !specification_has_unsatisfiable_assumptions(
                scored.specification, global_sat_checker())) {
            realizable_vec.push_back(scored.specification);
        }
    }
    return realizable_vec;
}

// Reduces realizable specifications to those maximal under implication, when
// run_implication_filter is enabled; otherwise deduplicates only.
std::vector<Specification> filter_maximal_specifications(
    const Config& cfg, const std::vector<Specification>& realizable_vec) {
    const auto impl_start = std::chrono::steady_clock::now();
    auto on_impl_progress = [&impl_start](std::size_t done, std::size_t total) {
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
    const std::vector<FilterFunction> filters =
        get_final_filter_functions(cfg, global_sat_checker(), on_impl_progress);
    const std::vector<Specification> result =
        filter_population(realizable_vec, filters);
    if (cfg.run_implication_filter) {
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
    }
    return result;
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
        << "  --config <file>      Path to a TOML configuration file.\n"
        << "                       Absent keys use built-in defaults.\n"
        << "  --format <fmt>       Input format: fretish or tlsf. If omitted,\n"
        << "                       inferred from the --input extension (a\n"
        << "                       .tlsf file is auto-detected as TLSF, any\n"
        << "                       other extension as FRETISH).\n"
        << "  --seed <n>           RNG seed for reproducible runs. If omitted\n"
        << "                       a random seed is chosen and printed.\n"
        << "  -h, --help           Show this help message and exit.\n"
        << "\n"
        << "Input format (examples/takeoff/spec.json):\n"
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
        << "Timing types: Immediately, NextTimepoint, Eventually, Always,\n"
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
    Config cfg;
    const std::optional<std::string> config_path =
        parse_string_arg(argc, argv, "--config");
    if (config_path.has_value()) {
        try {
            cfg = config_from_toml(*config_path);
        } catch (const std::exception& exc) {
            std::cerr << exc.what() << "\n";
            return 1;
        }
    }
    global_sat_checker().set_timeout(cfg.black_timeout);
    RealizabilityChecker::set_max_concurrency(cfg.max_concurrent_realizability);
    RealizabilityChecker::set_timeout(cfg.ltlsynt_timeout);
    set_ltl2tgba_timeout(cfg.ltl2tgba_timeout);
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
    const std::optional<std::string> format_arg =
        parse_string_arg(argc, argv, "--format");
    const bool is_tlsf =
        format_arg.has_value()
            ? *format_arg == "tlsf"
            : std::filesystem::path(*input_path).extension() == ".tlsf";
    if (is_tlsf) {
        const auto tlsf_wall_start = std::chrono::steady_clock::now();
        RandomSource tlsf_random_source = init_random_source(argc, argv);
        const std::optional<std::size_t> tlsf_seed = tlsf_random_source.seed();
        if (tlsf_seed.has_value()) {
            register_crash_metadata(
                format_crash_metadata(*tlsf_seed, *input_path, cfg));
        }
        const int tlsf_result =
            tlsf::run_repair(*input_path, *output_dir, cfg, tlsf_random_source);
        print_scoring_report();
        print_timing_report();
        if (cfg.report_cpu_timing) {
            const double wall_s =
                std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                              tlsf_wall_start)
                    .count();
            print_cpu_report(wall_s);
        }
        return tlsf_result;
    }
    Specification original_spec;
    try {
        original_spec = load_specification(*input_path);
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
    RandomSource random_source = init_random_source(argc, argv);
    const std::optional<std::size_t> maybe_seed = random_source.seed();
    if (!maybe_seed.has_value()) {
        std::cerr << "fatal: random source has no seed\n";
        return 1;
    }
    const std::size_t seed = *maybe_seed;
    std::cout << "Seed: " << seed << "\n";
    register_crash_metadata(format_crash_metadata(seed, *input_path, cfg));
    const auto wall_start = std::chrono::steady_clock::now();
    try {
        auto [population_result, filter_stats] =
            run_evolution(cfg, std::move(population), fitness_function,
                          filter_functions, random_source);
        population = std::move(population_result);
        const std::vector<Specification> realizable_vec =
            collect_realizable_specifications(population);
        const std::vector<Specification> maximal =
            filter_maximal_specifications(cfg, realizable_vec);
        const std::vector<ScoredSpecification> scored_maximal =
            score_and_sort_specifications(cfg, maximal, fitness_function);
        write_specifications(scored_maximal, fitness_function, *output_dir);
        std::cout << "Realizable specifications: " << realizable_vec.size();
        if (cfg.run_implication_filter) {
            std::cout << " (" << maximal.size() << " maximal)";
        }
        std::cout << ", written to " << *output_dir << "/\n";
        print_filter_report(filter_stats);
        print_scoring_report();
        print_timing_report();
        if (cfg.report_cpu_timing) {
            const double wall_s =
                std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                              wall_start)
                    .count();
            print_cpu_report(wall_s);
        }
    } catch (const std::exception& exc) {
        std::cerr << "fatal: " << exc.what() << "\n";
        return 1;
    }
    return 0;
}
