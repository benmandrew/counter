#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "genetic/generation.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Requirement make_req(const std::string& trigger, const std::string& response,
                     Timing timing = timing::immediately()) {
    return Requirement{Formula(trigger), Formula(response), timing};
}

RandomSource make_source(std::vector<std::size_t> values,
                         std::size_t fallback) {
    return RandomSource(
        [values = std::move(values), fallback,
         index = std::size_t{0}](std::size_t upper_bound) mutable {
            if (index >= values.size()) {
                return fallback % upper_bound;
            }
            const std::size_t value = values[index];
            ++index;
            return value % upper_bound;
        });
}

// --- score_population ---

void test_score_population_single_function() {
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("R", "S")};
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    const auto scored = score_population(pop, fns);
    expect(scored.size() == 2,
           "score_population: should score every requirement");
    expect(scored[0].fitness == 0.5,
           "score_population: single-function score should match return value");
    expect(scored[1].fitness == 0.5,
           "score_population: all equal fitness with constant function");
}

void test_score_population_weighted_aggregation() {
    const std::vector<Requirement> pop = {make_req("P", "Q")};
    // (0.0 * 1.0 + 1.0 * 3.0) / (1.0 + 3.0) == 0.75
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.0; }, 1.0},
        {[](const Requirement&) { return 1.0; }, 3.0}};
    const auto scored = score_population(pop, fns);
    expect(scored.size() == 1,
           "score_population: should produce one entry for a single-element "
           "population");
    expect(scored[0].fitness == 0.75,
           "score_population: should compute weighted average correctly");
}

void test_score_population_equal_weights_give_average() {
    const std::vector<Requirement> pop = {make_req("P", "Q")};
    // (0.2 * 1.0 + 0.8 * 1.0) / 2.0 == 0.5
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.2; }},
        {[](const Requirement&) { return 0.8; }}};
    const auto scored = score_population(pop, fns);
    expect(scored[0].fitness == 0.5,
           "score_population: equal weights should give arithmetic average");
}

void test_score_population_throws_on_empty_functions() {
    bool threw = false;
    try {
        score_population({make_req("P", "Q")}, {});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw,
           "score_population: should throw with no fitness functions provided");
}

void test_score_population_throws_on_zero_total_weight() {
    bool threw = false;
    try {
        score_population({make_req("P", "Q")},
                         {{[](const Requirement&) { return 0.5; }, 0.0}});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw,
           "score_population: should throw when total weight is not positive");
}

// --- make_predicate_filter / filter_population ---

void test_make_predicate_filter_keeps_matching() {
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("R", "S")};
    const FilterFunction f = make_predicate_filter(
        [](const Requirement& r) { return r.m_trigger.to_string() == "P"; });
    const auto survivors = f(pop);
    expect(survivors.size() == 1,
           "make_predicate_filter: should remove non-matching requirements");
    expect(survivors[0].m_trigger.to_string() == "P",
           "make_predicate_filter: should keep the matching requirement");
}

void test_filter_population_empty_filter_list_keeps_all() {
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("R", "S")};
    const auto survivors = filter_population(pop, {});
    expect(survivors.size() == 2,
           "filter_population: empty filter list should keep all requirements");
}

void test_filter_population_removes_failing() {
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("R", "S")};
    const std::vector<FilterFunction> filters = {make_predicate_filter(
        [](const Requirement& r) { return r.m_trigger.to_string() == "P"; })};
    const auto survivors = filter_population(pop, filters);
    expect(
        survivors.size() == 1,
        "filter_population: should remove requirements failing the predicate");
    expect(survivors[0].m_trigger.to_string() == "P",
           "filter_population: should keep the passing requirement");
}

void test_filter_population_applies_sequentially() {
    const std::vector<Requirement> pop = {
        make_req("P", "Q"), make_req("R", "S"), make_req("T", "U")};
    // First filter removes T; second filter removes R — only P survives.
    const std::vector<FilterFunction> filters = {
        make_predicate_filter([](const Requirement& r) {
            return r.m_trigger.to_string() != "T";
        }),
        make_predicate_filter([](const Requirement& r) {
            return r.m_trigger.to_string() != "R";
        })};
    const auto survivors = filter_population(pop, filters);
    expect(survivors.size() == 1,
           "filter_population: filters should be applied sequentially");
    expect(survivors[0].m_trigger.to_string() == "P",
           "filter_population: sequential filters should leave only P");
}

void test_filter_population_population_level_maximal_elements() {
    // Demonstrates a partial-order filter: keep only requirements with the
    // simplest (fewest-node) trigger formula — i.e., maximal elements under
    // "simpler trigger is better".
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("P & R", "Q")};
    const FilterFunction simplest_trigger =
        [](const std::vector<Requirement>& candidates) {
            if (candidates.empty()) return candidates;
            const std::size_t min_nodes =
                std::min_element(
                    candidates.begin(), candidates.end(),
                    [](const Requirement& a, const Requirement& b) {
                        return a.m_trigger.n_subformulae() <
                               b.m_trigger.n_subformulae();
                    })
                    ->m_trigger.n_subformulae();
            std::vector<Requirement> result;
            for (const Requirement& r : candidates) {
                if (r.m_trigger.n_subformulae() == min_nodes) {
                    result.push_back(r);
                }
            }
            return result;
        };
    const auto survivors = filter_population(pop, {simplest_trigger});
    expect(survivors.size() == 1,
           "filter_population: population-level filter should keep only "
           "maximal (simplest) elements");
    expect(survivors[0].m_trigger.to_string() == "P",
           "filter_population: should retain the requirement with the simpler "
           "trigger");
}

// --- evolve_generation ---

void test_evolve_generation_produces_target_size() {
    const std::vector<Requirement> pop = {
        make_req("P", "Q"), make_req("R", "S"), make_req("T", "U")};
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    const EvolutionConfig config{0.0, 0.0};
    const auto next_gen =
        evolve_generation(pop, 2, fns, {}, config, make_source({}, 0));
    expect(
        next_gen.size() == 2,
        "evolve_generation: should produce the requested number of offspring");
}

void test_evolve_generation_selects_fittest() {
    // P scores 0.9, R scores 0.1, T scores 0.5 — top 2 parents are P and T,
    // so with zero rates the offspring are unmodified copies of P and T.
    const std::vector<Requirement> pop = {
        make_req("P", "Q"), make_req("R", "S"), make_req("T", "U")};
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement& r) -> double {
            const auto t = r.m_trigger.to_string();
            if (t == "P") return 0.9;
            if (t == "T") return 0.5;
            return 0.1;
        }}};
    const EvolutionConfig config{0.0, 0.0};
    const auto next_gen =
        evolve_generation(pop, 2, fns, {}, config, make_source({}, 0));
    expect(next_gen.size() == 2,
           "evolve_generation: should return target_size offspring");
    expect(
        next_gen[0].m_trigger.to_string() == "P",
        "evolve_generation: fittest candidate should produce first offspring");
    expect(next_gen[1].m_trigger.to_string() == "T",
           "evolve_generation: second fittest should produce second offspring");
}

void test_evolve_generation_caps_at_survivor_count() {
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("R", "S")};
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    const EvolutionConfig config{0.0, 0.0};
    const auto next_gen =
        evolve_generation(pop, 5, fns, {}, config, make_source({}, 0));
    expect(next_gen.size() == 2,
           "evolve_generation: should return at most the number of survivors "
           "when target_size exceeds population");
}

void test_evolve_generation_applies_filter_before_selection() {
    // Filter removes R; only P survives, so the result must come from P.
    const std::vector<Requirement> pop = {make_req("P", "Q"),
                                          make_req("R", "S")};
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    const std::vector<FilterFunction> filters = {make_predicate_filter(
        [](const Requirement& r) { return r.m_trigger.to_string() == "P"; })};
    const EvolutionConfig config{0.0, 0.0};
    const auto next_gen =
        evolve_generation(pop, 2, fns, filters, config, make_source({}, 0));
    expect(next_gen.size() == 1,
           "evolve_generation: filtered-out requirements must not appear");
    expect(next_gen[0].m_trigger.to_string() == "P",
           "evolve_generation: only the surviving requirement should be used");
}

void test_evolve_generation_throws_with_no_fitness_functions() {
    bool threw = false;
    try {
        evolve_generation({make_req("P", "Q")}, 1, {}, {},
                          EvolutionConfig{1.0, 1.0}, make_source({}, 0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw,
           "evolve_generation: should throw when no fitness functions are "
           "provided");
}

void test_evolve_generation_throws_when_all_filtered_out() {
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    const std::vector<FilterFunction> filters = {
        [](const std::vector<Requirement>&) {
            return std::vector<Requirement>{};
        }};
    bool threw = false;
    try {
        evolve_generation({make_req("P", "Q")}, 1, fns, filters,
                          EvolutionConfig{1.0, 1.0}, make_source({}, 0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw,
           "evolve_generation: should throw when all requirements are filtered "
           "out");
}

void test_evolve_generation_throws_on_invalid_crossover_rate() {
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    bool threw = false;
    try {
        evolve_generation({make_req("P", "Q")}, 1, fns, {},
                          EvolutionConfig{1.5, 0.5}, make_source({}, 0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw,
           "evolve_generation: crossover_rate outside [0,1] should throw");
}

void test_evolve_generation_throws_on_invalid_mutation_rate() {
    const std::vector<WeightedFitnessFunction> fns = {
        {[](const Requirement&) { return 0.5; }}};
    bool threw = false;
    try {
        evolve_generation({make_req("P", "Q")}, 1, fns, {},
                          EvolutionConfig{0.5, -0.1}, make_source({}, 0));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw,
           "evolve_generation: mutation_rate outside [0,1] should throw");
}

}  // namespace

void run_generation_tests() {
    test_score_population_single_function();
    test_score_population_weighted_aggregation();
    test_score_population_equal_weights_give_average();
    test_score_population_throws_on_empty_functions();
    test_score_population_throws_on_zero_total_weight();
    test_make_predicate_filter_keeps_matching();
    test_filter_population_empty_filter_list_keeps_all();
    test_filter_population_removes_failing();
    test_filter_population_applies_sequentially();
    test_filter_population_population_level_maximal_elements();
    test_evolve_generation_produces_target_size();
    test_evolve_generation_selects_fittest();
    test_evolve_generation_caps_at_survivor_count();
    test_evolve_generation_applies_filter_before_selection();
    test_evolve_generation_throws_with_no_fitness_functions();
    test_evolve_generation_throws_when_all_filtered_out();
    test_evolve_generation_throws_on_invalid_crossover_rate();
    test_evolve_generation_throws_on_invalid_mutation_rate();
}
