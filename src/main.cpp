#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "crash/crash_handler.hpp"
#include "filter/implication.hpp"
#include "fitness/halstead.hpp"
#include "fitness/semantic_similarity.hpp"
#include "fitness/status.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/ganak.hpp"
#include "runner/spot.hpp"

AggregateWeightedFitnessFunction get_fitness_function(
    const Specification& original_spec) {
    std::vector<WeightedFitnessFunction> fitness_functions{};
    if (Config::fitness_weight_syntactic > 0.0) {
        auto synsim = [original_spec](const Specification& spec) -> double {
            return syntactic_similarity(spec, original_spec);
        };
        fitness_functions.push_back(
            {synsim, Config::fitness_weight_syntactic, "syntactic"});
    }
    if (Config::fitness_weight_semantic > 0.0) {
        auto semsim = [original_spec](const Specification& spec) -> double {
            return semantic_similarity(spec, original_spec, 10);
        };
        fitness_functions.push_back(
            {semsim, Config::fitness_weight_semantic, "semantic"});
    }
    if (Config::fitness_weight_halstead > 0.0) {
        auto halstead = [original_spec](const Specification& spec) -> double {
            return halstead_fitness(spec, original_spec);
        };
        fitness_functions.push_back(
            {halstead, Config::fitness_weight_halstead, "halstead"});
    }
    if (Config::fitness_weight_status > 0.0) {
        auto status = [](const Specification& spec) -> double {
            return specification_status(spec, global_sat_checker(),
                                        global_real_checker());
        };
        fitness_functions.push_back(
            {status, Config::fitness_weight_status, "status"});
    }
    return AggregateWeightedFitnessFunction(std::move(fitness_functions));
}

std::vector<FilterFunction> get_filter_functions() {
    return {
        // A false trigger is vacuously satisfied by every trace, so it
        // imposes no constraint; forbid it from surviving as a breeding
        // candidate rather than letting the fitness function alone
        // discourage it.
        make_predicate_filter([](const Specification& spec) {
            return !specification_has_false_trigger(spec);
        }),
    };
}

// Specification get_spec() {
//     std::vector<Requirement> assumptions = {};
//     std::vector<Requirement> guarantees = {
//         Requirement(Formula("r1"), Formula("g1"), timing::eventually(),
//                     "G(r1 -> F g1)"),
//         Requirement(Formula("r2"), Formula("g2"), timing::eventually(),
//                     "G(r2 -> F g2)"),
//         Requirement(Formula("!a"), Formula("!g1 & !g2"),
//         timing::immediately(),
//                     "G(!a -> (!g1 & !g2))"),
//     };
//     std::vector<std::string> in_atoms = {"a", "r1", "r2"};
//     std::vector<std::string> out_atoms = {"g1", "g2"};
//     return Specification(assumptions, guarantees, in_atoms, out_atoms);
// }

Specification get_spec() {
    std::vector<Requirement> assumptions = {};
    std::vector<Requirement> guarantees = {
        Requirement(Formula("true"), Formula("takeoff_roll"),
                    timing::for_ticks(5)),
        Requirement(Formula("true"), Formula("!takeoff_roll & lift_off"),
                    timing::within_ticks(5)),
        Requirement(Formula("takeoff_roll"), Formula("lift_off"),
                    timing::after_ticks(1)),
    };
    std::vector<std::string> in_atoms = {};
    std::vector<std::string> out_atoms = {"takeoff_roll", "lift_off"};
    return Specification(assumptions, guarantees, in_atoms, out_atoms);
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

std::string format_crash_metadata(std::size_t seed) {
    std::ostringstream out;
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
    std::cout << "Seed: " << seed << "\n";
    register_crash_metadata(format_crash_metadata(seed));
    return make_random_source_from_seed(seed);
}

std::vector<ScoredSpecification> run_evolution(
    std::vector<ScoredSpecification> population,
    const AggregateWeightedFitnessFunction& fitness_function,
    const std::vector<FilterFunction>& filter_functions,
    RandomSource& random_source) {
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
    }
    return population;
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
        std::cout << "\rImplication filter: " << std::setw(3)
                  << (done * 100 / total) << "%  " << std::fixed
                  << std::setprecision(2) << elapsed << "s" << std::flush;
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
              << ImplicationFilterStats::n_comparisons << " comparisons, "
              << ImplicationFilterStats::n_skipped
              << " skipped via subsumption, "
              << ImplicationFilterStats::n_duplicates
              << " duplicate specs excluded)\n";
    return maximal;
}

std::vector<ScoredSpecification> score_and_sort_specifications(
    const std::vector<Specification>& specs,
    const AggregateWeightedFitnessFunction& fitness_function) {
    std::vector<ScoredSpecification> scored;
    scored.reserve(specs.size());
    for (const Specification& spec : specs) {
        scored.push_back({spec, fitness_function(spec)});
    }
    std::sort(scored.begin(), scored.end(),
              [](const ScoredSpecification& first,
                 const ScoredSpecification& second) {
                  return first.fitness > second.fitness;
              });
    return scored;
}

void print_top_specifications(
    const std::vector<ScoredSpecification>& scored_maximal,
    const AggregateWeightedFitnessFunction& fitness_function,
    std::size_t realizable_count) {
    const std::size_t print_count =
        std::min(scored_maximal.size(), std::size_t{5});
    std::cout << "\nRealizable specifications after " << Config::generations
              << " generations (";
    if (Config::run_implication_filter) {
        std::cout << scored_maximal.size() << " reduced from "
                  << realizable_count;
    } else {
        std::cout << scored_maximal.size();
    }
    std::cout << "):\n";
    for (std::size_t i = 0; i < print_count; ++i) {
        const Specification& spec = scored_maximal[i].specification;
        std::cout << "Fitness: " << std::fixed << std::setprecision(4)
                  << scored_maximal[i].fitness << "\n";
        for (const WeightedFitnessFunction& wff : fitness_function) {
            std::cout << "  " << std::left << std::setw(10) << wff.name
                      << " (w=" << wff.weight << "): " << std::fixed
                      << std::setprecision(4) << wff.function(spec) << "\n";
        }
        std::cout << spec.to_string() << "\n\n";
    }
}

int main(int argc, const char* const argv[]) {
    if (argc == 0 || argv == nullptr || argv[0] == nullptr) {
        std::cerr << "fatal: missing argv[0]\n";
        return 1;
    }
    init_cpptrace(argv[0]);
    Specification original_spec = get_spec();
    std::cout << "Original specification:\n"
              << original_spec.to_string() << "\n";
    AggregateWeightedFitnessFunction fitness_function =
        get_fitness_function(original_spec);
    const std::vector<FilterFunction> filter_functions = get_filter_functions();
    std::vector<ScoredSpecification> population = original_population(
        original_spec, fitness_function, Config::population_size);
    RandomSource random_source = init_random_source(argc, argv);
    population = run_evolution(std::move(population), fitness_function,
                               filter_functions, random_source);
    const std::vector<Specification> realizable_vec =
        collect_realizable_specifications(population);
    const std::vector<Specification> maximal =
        filter_maximal_specifications(realizable_vec);
    const std::vector<ScoredSpecification> scored_maximal =
        score_and_sort_specifications(maximal, fitness_function);
    print_top_specifications(scored_maximal, fitness_function,
                             realizable_vec.size());
    print_timing_report();
    return 0;
}
