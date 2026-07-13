#include <functional>
#include <string>
#include <vector>

#include "fitness/function.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Specification make_spec(const std::string& trigger,
                        const std::string& response) {
    return Specification({},
                         {Requirement{Formula(trigger), Formula(response),
                                      timing::immediately()}},
                         {}, {});
}

// --- std::hash<Specification> ---

void test_hash_identical_specifications_are_equal() {
    const Specification spec = make_spec("p", "q");
    expect(std::hash<Specification>{}(spec) == std::hash<Specification>{}(spec),
           "hash: identical specifications must hash to the same value");
}

void test_hash_equal_specifications_are_equal() {
    const Specification spec_a = make_spec("p", "q");
    const Specification spec_b = make_spec("p", "q");
    expect(std::hash<Specification>{}(spec_a) ==
               std::hash<Specification>{}(spec_b),
           "hash: structurally equal specifications must hash to the same "
           "value");
}

void test_hash_different_specifications_differ() {
    const Specification spec_a = make_spec("p", "q");
    const Specification spec_b = make_spec("r", "s");
    expect(std::hash<Specification>{}(spec_a) !=
               std::hash<Specification>{}(spec_b),
           "hash: distinct specifications should (almost always) hash "
           "differently");
}

// --- AggregateWeightedFitnessFunction memoisation ---

void test_fitness_function_memoises_repeated_calls() {
    int call_count = 0;
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[&call_count](const Specification&) {
              ++call_count;
              return 0.5;
          },
          1.0, ""}});
    const Specification spec = make_spec("p", "q");
    fitness_fn(spec);
    fitness_fn(spec);
    expect(call_count == 1,
           "fitness: identical specification should be scored only once");
}

void test_fitness_function_scores_distinct_specs_independently() {
    int call_count = 0;
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[&call_count](const Specification&) {
              ++call_count;
              return 0.5;
          },
          1.0, ""}});
    fitness_fn(make_spec("p", "q"));
    fitness_fn(make_spec("r", "s"));
    expect(call_count == 2,
           "fitness: distinct specifications should each be scored once");
}

void test_fitness_function_cached_value_is_correct() {
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[](const Specification&) { return 0.75; }, 1.0, ""}});
    const Specification spec = make_spec("p", "q");
    const double first_result = fitness_fn(spec);
    const double second_result = fitness_fn(spec);
    expect(first_result == second_result,
           "fitness: cached result must equal the originally computed value");
    expect(first_result == 0.75,
           "fitness: cached result must be the correct weighted-average score");
}

// --- per-objective exposure ---

void test_objectives_returns_raw_component_scores_in_order() {
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[](const Specification&) { return 0.25; }, 3.0, "a"},
         {[](const Specification&) { return 0.75; }, 1.0, "b"}});
    const std::vector<double> objectives =
        fitness_fn.objectives(make_spec("p", "q"));
    expect(objectives.size() == 2,
           "objectives: one entry per aggregated function");
    expect(
        objectives[0] == 0.25 && objectives[1] == 0.75,
        "objectives: raw component scores, unweighted, in registration order");
}

void test_operator_is_weighted_average_of_objectives() {
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[](const Specification&) { return 0.25; }, 3.0, "a"},
         {[](const Specification&) { return 0.75; }, 1.0, "b"}});
    // (3*0.25 + 1*0.75) / (3+1) = 1.5 / 4 = 0.375 (exact in binary)
    expect(fitness_fn(make_spec("p", "q")) == 0.375,
           "operator(): weighted average over the raw objective scores");
}

void test_objectives_and_operator_share_one_evaluation() {
    int call_count = 0;
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[&call_count](const Specification&) {
              ++call_count;
              return 0.5;
          },
          1.0, ""}});
    const Specification spec = make_spec("p", "q");
    fitness_fn.objectives(spec);
    fitness_fn(spec);
    fitness_fn.objectives_and_fitness(spec);
    expect(call_count == 1,
           "objectives/operator/objectives_and_fitness: a spec is evaluated "
           "once and served from one shared cache");
}

void test_objectives_and_fitness_matches_separate_calls() {
    const AggregateWeightedFitnessFunction fitness_fn(
        {{[](const Specification&) { return 0.25; }, 3.0, "a"},
         {[](const Specification&) { return 0.75; }, 1.0, "b"}});
    const Specification spec = make_spec("p", "q");
    const auto [objectives, fitness] = fitness_fn.objectives_and_fitness(spec);
    expect(objectives == fitness_fn.objectives(spec),
           "objectives_and_fitness: vector matches objectives()");
    expect(fitness == fitness_fn(spec),
           "objectives_and_fitness: scalar matches operator()");
    expect(fitness_fn.n_objectives() == 2,
           "n_objectives: reports the number of aggregated functions");
}

}  // namespace

void run_fitness_function_tests() {
    test_hash_identical_specifications_are_equal();
    test_hash_equal_specifications_are_equal();
    test_hash_different_specifications_differ();
    test_fitness_function_memoises_repeated_calls();
    test_fitness_function_scores_distinct_specs_independently();
    test_fitness_function_cached_value_is_correct();
    test_objectives_returns_raw_component_scores_in_order();
    test_operator_is_weighted_average_of_objectives();
    test_objectives_and_operator_share_one_evaluation();
    test_objectives_and_fitness_matches_separate_calls();
}
