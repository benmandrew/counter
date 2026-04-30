#include <algorithm>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "fitness/syntactic_similarity.hpp"
#include "genetic/generation.hpp"
#include "requirement.hpp"

std::vector<WeightedFitnessFunction> get_fitness_functions(
    const Specification& original_spec) {
    auto fitness_fn = [original_spec](const Specification& spec) -> double {
        return syntactic_similarity(spec, original_spec);
    };
    return {{fitness_fn, 1.0}};
}

int main() {
    Specification original_spec(
        {Requirement(Formula("p"), Formula("q"), timing::immediately()),
         Requirement(Formula("p"), Formula("!q"), timing::immediately())},
        {"p"}, {"q"});
    // 1. Initial population — each requirement wrapped in a specification
    std::vector<Specification> population = {
        Specification{
            {Requirement(Formula("p"), Formula("q"), timing::immediately()),
             Requirement(Formula("p"), Formula("!q"), timing::immediately())},
            {"p"},
            {"q"}},
        Specification{
            {Requirement(Formula("p"), Formula("q"), timing::immediately()),
             Requirement(Formula("p"), Formula("!q"), timing::immediately())},
            {"p"},
            {"q"}},
        Specification{
            {Requirement(Formula("p"), Formula("q"), timing::immediately()),
             Requirement(Formula("p"), Formula("!q"), timing::immediately())},
            {"p"},
            {"q"}},
        Specification{
            {Requirement(Formula("p"), Formula("q"), timing::immediately()),
             Requirement(Formula("p"), Formula("!q"), timing::immediately())},
            {"p"},
            {"q"}},
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
        const Requirement& best =
            *scored.front().specification.m_requirements.begin();
        std::cout << "Trigger: " << best.m_trigger.to_string()
                  << "\nResponse: " << best.m_response.to_string()
                  << "\nTiming: " << to_string(best.m_timing)
                  << "\nFitness: " << scored.front().fitness << "\n";
    }
    return 0;
}
