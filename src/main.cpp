#include <algorithm>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "fitness/semantic_similarity.hpp"
#include "fitness/status.hpp"
#include "fitness/syntactic_similarity.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"

std::vector<WeightedFitnessFunction> get_fitness_functions(
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
    return {{synsim, 0.5}, {semsim, 0.3}, {status, 0.2}};
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

int main() {
    Specification original_spec = get_spec();
    // 1. Initial population — each requirement wrapped in a specification
    std::vector<Specification> population = {
        original_spec,
        original_spec,
        original_spec,
        original_spec,
    };
    // 2. Fitness functions
    std::vector<WeightedFitnessFunction> fitness_functions =
        get_fitness_functions(original_spec);
    // 3. No filtering for demo
    std::vector<FilterFunction> filters;

    // 4. Evolution config
    EvolutionConfig config;
    // 5. Random source
    std::random_device rd;
    RandomSource random_source = make_random_source_from_seed(rd());
    // 6. Run genetic algorithm for a few generations
    std::size_t generations = 10;
    std::size_t pop_size = population.size();
    for (std::size_t gen_idx = 0; gen_idx < generations; ++gen_idx) {
        std::cout << "Generation " << gen_idx + 1 << ":\n";
        population = evolve_generation(population, pop_size, fitness_functions,
                                       filters, config, random_source);
    }
    // 7. Score and print the best specification
    auto scored = score_population(population, fitness_functions);
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
        return a.fitness > b.fitness;
    });
    std::cout << "Best specification after " << generations
              << " generations:\n";
    if (!scored.empty()) {
        for (const Requirement& req :
             scored.front().specification.m_guarantees) {
            std::cout << "Requirement:\n  Trigger: "
                      << req.m_trigger.to_string()
                      << "\n  Response: " << req.m_response.to_string()
                      << "\n  Timing: " << to_string(req.m_timing)
                      << "\n  Fitness: " << scored.front().fitness << "\n";
        }
    }
    return 0;
}
