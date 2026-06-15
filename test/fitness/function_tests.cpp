#include <functional>
#include <string>

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

}  // namespace

void run_fitness_function_tests() {
    test_hash_identical_specifications_are_equal();
    test_hash_equal_specifications_are_equal();
    test_hash_different_specifications_differ();
    test_fitness_function_memoises_repeated_calls();
    test_fitness_function_scores_distinct_specs_independently();
    test_fitness_function_cached_value_is_correct();
}
