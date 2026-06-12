#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

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
    auto synsim = [original_spec](const Specification& spec) -> double {
        return syntactic_similarity(spec, original_spec);
    };
    auto semsim = [original_spec](const Specification& spec) -> double {
        return semantic_similarity(spec, original_spec);
    };
    auto status = [](const Specification& spec) -> double {
        return specification_status(spec);
    };
    return AggregateWeightedFitnessFunction(
        {{synsim, 0.15}, {semsim, 0.15}, {status, 0.7}});
}

Specification get_spec() {
    std::vector<Requirement> assumptions = {};
    std::vector<Requirement> guarantees = {
        Requirement(Formula("r1"), Formula("g1"), timing::eventually(),
                    "G(r1 -> F g1)"),
        Requirement(Formula("r2"), Formula("g2"), timing::eventually(),
                    "G(r2 -> F g2)"),
        Requirement(Formula("!a"), Formula("!g1 & !g2"), timing::immediately(),
                    "G(!a -> (!g1 & !g2))"),
    };
    std::vector<std::string> in_atoms = {"a", "r1", "r2"};
    std::vector<std::string> out_atoms = {"g1", "g2"};
    return Specification(assumptions, guarantees, in_atoms, out_atoms);
}

void print_timing_report() {
    auto print_row = [](const char* name, std::size_t calls, double total_s,
                        std::size_t cache_hits) {
        const double avg_s =
            calls > 0 ? total_s / static_cast<double>(calls) : 0.0;
        std::cout << std::left << std::setw(12) << name << std::right
                  << std::setw(6) << calls << " calls  " << std::fixed
                  << std::setprecision(3) << std::setw(8) << total_s
                  << "s total  " << std::setw(8) << avg_s << "s avg";
        if (cache_hits > 0) {
            std::cout << "  (+" << cache_hits << " cache hits)";
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
              SatisfiabilityChecker::n_cache_hits);
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

constexpr std::size_t generations = 10;
constexpr std::size_t population_size = 20;

int main() {
    Specification original_spec = get_spec();
    std::cout << "Original specification:\n"
              << original_spec.to_string() << "\n";
    // 1. Fitness functions
    AggregateWeightedFitnessFunction fitness_function =
        get_fitness_function(original_spec);
    // 2. Initial population — each requirement wrapped in a specification
    std::vector<ScoredSpecification> population =
        original_population(original_spec, fitness_function, population_size);
    // 3. No filtering for demo
    std::vector<FilterFunction> filters;
    // 4. Evolution config
    EvolutionConfig config;
    // 5. Random source
    std::random_device rng_dev;
    RandomSource random_source = make_random_source_from_seed(rng_dev());
    // 6. Run genetic algorithm for a few generations
    std::size_t pop_size = population.size();

    for (std::size_t gen_idx = 0; gen_idx < generations; ++gen_idx) {
        std::cout << "Generation " << gen_idx + 1 << ": " << std::flush;
        const auto start = std::chrono::steady_clock::now();
        population = evolve_generation(population, pop_size, fitness_function,
                                       filters, config, random_source);
        const double elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
        std::cout << std::fixed << std::setprecision(2) << elapsed << "s\n";
    }
    // 7. Collect and print all unique realizable specifications
    std::set<Specification> realizable;
    for (const ScoredSpecification& scored : population) {
        if (specification_status(scored.specification) == 1.0) {
            realizable.insert(scored.specification);
        }
    }
    std::cout << "\nRealizable specifications after " << generations
              << " generations (" << realizable.size() << "):\n";
    for (const Specification& spec : realizable) {
        std::cout << spec.to_string() << "\n\n";
    }
    print_timing_report();
    return 0;
}
