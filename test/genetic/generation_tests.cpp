#include <algorithm>
#include <set>
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

Specification make_spec(const std::string& trigger, const std::string& response,
                        Timing timing = timing::immediately()) {
    return Specification({}, {make_req(trigger, response, timing)}, {}, {});
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

std::string first_trigger(const Specification& spec) {
    return spec.m_guarantees.begin()->m_trigger.to_string();
}

// --- score_population ---

void test_score_population_single_function() {
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("r", "s")};
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});
    const auto scored = score_population(pop, fns);
    expect(scored.size() == 2,
           "score_population: should score every specification");
    expect(scored[0].fitness == 0.5,
           "score_population: single-function score should match return value");
    expect(scored[1].fitness == 0.5,
           "score_population: all equal fitness with constant function");
}

void test_score_population_weighted_aggregation() {
    const std::vector<Specification> pop = {make_spec("p", "q")};
    // (0.0 * 1.0 + 1.0 * 3.0) / (1.0 + 3.0) == 0.75
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.0; }, 1.0, ""},
             {[](const Specification&) { return 1.0; }, 3.0, ""}});
    const auto scored = score_population(pop, fns);
    expect(scored.size() == 1,
           "score_population: should produce one entry for a single-element "
           "population");
    expect(scored[0].fitness == 0.75,
           "score_population: should compute weighted average correctly");
}

void test_score_population_equal_weights_give_average() {
    const std::vector<Specification> pop = {make_spec("p", "q")};
    // (0.2 * 1.0 + 0.8 * 1.0) / 2.0 == 0.5
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.2; }, 1.0, ""},
             {[](const Specification&) { return 0.8; }, 1.0, ""}});
    const auto scored = score_population(pop, fns);
    expect(scored[0].fitness == 0.5,
           "score_population: equal weights should give arithmetic average");
}

// --- make_predicate_filter / filter_population ---

void test_make_predicate_filter_keeps_matching() {
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("r", "s")};
    const FilterFunction filter = make_predicate_filter(
        "",
        [](const Specification& spec) { return first_trigger(spec) == "p"; });
    const auto survivors = filter(pop);
    expect(survivors.size() == 1,
           "make_predicate_filter: should remove non-matching specifications");
    expect(first_trigger(survivors[0]) == "p",
           "make_predicate_filter: should keep the matching specification");
}

void test_filter_population_empty_filter_list_keeps_all() {
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("r", "s")};
    const auto survivors = filter_population(pop, {});
    expect(
        survivors.size() == 2,
        "filter_population: empty filter list should keep all specifications");
}

void test_filter_population_removes_failing() {
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("r", "s")};
    const std::vector<FilterFunction> filters = {make_predicate_filter(
        "",
        [](const Specification& spec) { return first_trigger(spec) == "p"; })};
    const auto survivors = filter_population(pop, filters);
    expect(survivors.size() == 1,
           "filter_population: should remove specifications failing the "
           "predicate");
    expect(first_trigger(survivors[0]) == "p",
           "filter_population: should keep the passing specification");
}

void test_filter_population_applies_sequentially() {
    const std::vector<Specification> pop = {
        make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")};
    // First filter removes t; second filter removes r — only p survives.
    const std::vector<FilterFunction> filters = {
        make_predicate_filter("",
                              [](const Specification& spec) {
                                  return first_trigger(spec) != "t";
                              }),
        make_predicate_filter("", [](const Specification& spec) {
            return first_trigger(spec) != "r";
        })};
    const auto survivors = filter_population(pop, filters);
    expect(survivors.size() == 1,
           "filter_population: filters should be applied sequentially");
    expect(first_trigger(survivors[0]) == "p",
           "filter_population: sequential filters should leave only p");
}

void test_filter_population_population_level_maximal_elements() {
    // Keep only specs with the simplest (fewest-node) trigger formula.
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("p & r", "q")};
    const FilterFunction simplest_trigger = [](const std::vector<Specification>&
                                                   candidates) {
        if (candidates.empty()) {
            return candidates;
        }
        const std::size_t min_nodes =
            std::min_element(
                candidates.begin(), candidates.end(),
                [](const Specification& lhs, const Specification& rhs) {
                    return lhs.m_guarantees.begin()->m_trigger.n_subformulae() <
                           rhs.m_guarantees.begin()->m_trigger.n_subformulae();
                })
                ->m_guarantees.begin()
                ->m_trigger.n_subformulae();
        std::vector<Specification> result;
        for (const Specification& spec : candidates) {
            if (spec.m_guarantees.begin()->m_trigger.n_subformulae() ==
                min_nodes) {
                result.push_back(spec);
            }
        }
        return result;
    };
    const auto survivors = filter_population(pop, {simplest_trigger});
    expect(survivors.size() == 1,
           "filter_population: population-level filter should keep only "
           "maximal (simplest) elements");
    expect(first_trigger(survivors[0]) == "p",
           "filter_population: should retain the specification with the "
           "simpler trigger");
}

// --- evolve_generation ---

void test_evolve_generation_produces_target_size() {
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});

    const std::vector<ScoredSpecification> pop = score_population(
        {make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")}, fns);
    const auto next_gen =
        evolve_generation(pop, 2, fns, {}, make_source({}, 0));
    expect(
        next_gen.size() == 2,
        "evolve_generation: should produce the requested number of offspring");
}

void test_evolve_generation_pads_up_to_target_size() {
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});
    const std::vector<ScoredSpecification> pop =
        score_population({make_spec("p", "q"), make_spec("r", "s")}, fns);
    const auto next_gen =
        evolve_generation(pop, 5, fns, {}, make_source({}, 0));
    expect(next_gen.size() == 5,
           "evolve_generation: should pad the next generation back to the "
           "requested target size");
}

void test_evolve_generation_selects_parents_before_offspring_filtering() {
    // The filter is applied after breeding, and the generation is then padded
    // back to the requested size if filtering shrinks the offspring pool.
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});
    const std::vector<ScoredSpecification> pop =
        score_population({make_spec("p", "q"), make_spec("r", "s")}, fns);
    const std::vector<FilterFunction> filters = {
        [](const std::vector<Specification>& candidates) {
            if (candidates.empty()) {
                return candidates;
            }
            return std::vector<Specification>{candidates.front()};
        }};
    const auto next_gen =
        evolve_generation(pop, 2, fns, filters, make_source({}, 0));
    expect(next_gen.size() == 2,
           "evolve_generation: the generation should be padded back to the "
           "requested target size after filtering");
    expect(first_trigger(next_gen[0].specification) ==
               first_trigger(next_gen[1].specification),
           "evolve_generation: padded offspring should duplicate the "
           "surviving specification");
}

}  // namespace

void run_generation_tests() {
    test_score_population_single_function();
    test_score_population_weighted_aggregation();
    test_score_population_equal_weights_give_average();
    test_make_predicate_filter_keeps_matching();
    test_filter_population_empty_filter_list_keeps_all();
    test_filter_population_removes_failing();
    test_filter_population_applies_sequentially();
    test_filter_population_population_level_maximal_elements();
    test_evolve_generation_produces_target_size();
    test_evolve_generation_pads_up_to_target_size();
    test_evolve_generation_selects_parents_before_offspring_filtering();
}
