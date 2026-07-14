#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
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

std::string first_condition(const Specification& spec) {
    return spec.m_guarantees.begin()->m_condition.to_string();
}

// --- score_population ---

void test_score_population_single_function() {
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("r", "s")};
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});
    const auto scored = score_population(Config{}, pop, fns);
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
    const auto scored = score_population(Config{}, pop, fns);
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
    const auto scored = score_population(Config{}, pop, fns);
    expect(scored[0].fitness == 0.5,
           "score_population: equal weights should give arithmetic average");
}

void test_score_population_drops_failing_individual() {
    const std::vector<Specification> pop = {
        make_spec("p", "q"), make_spec("boom", "q"), make_spec("r", "s")};
    // Mimics an external tool failing on one evolved formula: the individual
    // is dropped, the rest of the generation is scored as normal.
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction({{[](const Specification& spec) {
                                               if (first_condition(spec) ==
                                                   "boom") {
                                                   throw std::runtime_error(
                                                       "tool exited with code "
                                                       "2");
                                               }
                                               return 0.5;
                                           },
                                           1.0, ""}});
    const auto scored = score_population(Config{}, pop, fns);
    expect(scored.size() == 2,
           "score_population: should drop the individual that threw");
    expect(first_condition(scored[0].specification) == "p" &&
               first_condition(scored[1].specification) == "r",
           "score_population: survivors should keep their relative order");
}

void test_score_population_circuit_breaker_trips() {
    const std::vector<Specification> pop = {
        make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")};
    // Every individual fails, as it would with a missing or broken tool. That
    // must abort rather than quietly evolving an empty population.
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction({{[](const Specification&) -> double {
                                               throw std::runtime_error(
                                                   "tool not found");
                                           },
                                           1.0, ""}});
    bool threw = false;
    try {
        score_population(Config{}, pop, fns);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    expect(threw,
           "score_population: should abort when the whole generation fails");
}

// --- make_predicate_filter / filter_population ---

void test_make_predicate_filter_keeps_matching() {
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("r", "s")};
    const FilterFunction filter = make_predicate_filter(
        "",
        [](const Specification& spec) { return first_condition(spec) == "p"; });
    const auto survivors = filter(pop);
    expect(survivors.size() == 1,
           "make_predicate_filter: should remove non-matching specifications");
    expect(first_condition(survivors[0]) == "p",
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
    const std::vector<FilterFunction> filters = {
        make_predicate_filter("", [](const Specification& spec) {
            return first_condition(spec) == "p";
        })};
    const auto survivors = filter_population(pop, filters);
    expect(survivors.size() == 1,
           "filter_population: should remove specifications failing the "
           "predicate");
    expect(first_condition(survivors[0]) == "p",
           "filter_population: should keep the passing specification");
}

void test_filter_population_applies_sequentially() {
    const std::vector<Specification> pop = {
        make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")};
    // First filter removes t; second filter removes r — only p survives.
    const std::vector<FilterFunction> filters = {
        make_predicate_filter("",
                              [](const Specification& spec) {
                                  return first_condition(spec) != "t";
                              }),
        make_predicate_filter("", [](const Specification& spec) {
            return first_condition(spec) != "r";
        })};
    const auto survivors = filter_population(pop, filters);
    expect(survivors.size() == 1,
           "filter_population: filters should be applied sequentially");
    expect(first_condition(survivors[0]) == "p",
           "filter_population: sequential filters should leave only p");
}

void test_filter_population_population_level_maximal_elements() {
    // Keep only specs with the simplest (fewest-node) condition formula.
    const std::vector<Specification> pop = {make_spec("p", "q"),
                                            make_spec("p & r", "q")};
    const FilterFunction simplest_condition =
        [](const std::vector<Specification>& candidates) {
            if (candidates.empty()) {
                return candidates;
            }
            const std::size_t min_nodes =
                std::min_element(
                    candidates.begin(), candidates.end(),
                    [](const Specification& lhs, const Specification& rhs) {
                        return lhs.m_guarantees.begin()
                                   ->m_condition.n_subformulae() <
                               rhs.m_guarantees.begin()
                                   ->m_condition.n_subformulae();
                    })
                    ->m_guarantees.begin()
                    ->m_condition.n_subformulae();
            std::vector<Specification> result;
            for (const Specification& spec : candidates) {
                if (spec.m_guarantees.begin()->m_condition.n_subformulae() ==
                    min_nodes) {
                    result.push_back(spec);
                }
            }
            return result;
        };
    const auto survivors = filter_population(pop, {simplest_condition});
    expect(survivors.size() == 1,
           "filter_population: population-level filter should keep only "
           "maximal (simplest) elements");
    expect(first_condition(survivors[0]) == "p",
           "filter_population: should retain the specification with the "
           "simpler condition");
}

// --- evolve_generation ---

void test_evolve_generation_produces_target_size() {
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});

    const std::vector<ScoredSpecification> pop = score_population(
        Config{},
        {make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")}, fns);
    const auto next_gen =
        evolve_generation(Config{}, pop, 2, 0, fns, {}, make_source({}, 0));
    expect(
        next_gen.size() == 2,
        "evolve_generation: should produce the requested number of offspring");
}

void test_evolve_generation_pads_up_to_target_size() {
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction(
            {{[](const Specification&) { return 0.5; }, 1.0, ""}});
    const std::vector<ScoredSpecification> pop = score_population(
        Config{}, {make_spec("p", "q"), make_spec("r", "s")}, fns);
    const auto next_gen =
        evolve_generation(Config{}, pop, 5, 0, fns, {}, make_source({}, 0));
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
    const std::vector<ScoredSpecification> pop = score_population(
        Config{}, {make_spec("p", "q"), make_spec("r", "s")}, fns);
    const std::vector<FilterFunction> filters = {
        [](const std::vector<Specification>& candidates) {
            if (candidates.empty()) {
                return candidates;
            }
            return std::vector<Specification>{candidates.front()};
        }};
    const auto next_gen = evolve_generation(Config{}, pop, 2, 0, fns, filters,
                                            make_source({}, 0));
    expect(next_gen.size() == 2,
           "evolve_generation: the generation should be padded back to the "
           "requested target size after filtering");
    expect(first_condition(next_gen[0].specification) ==
               first_condition(next_gen[1].specification),
           "evolve_generation: padded offspring should duplicate the "
           "surviving specification");
}

void test_evolve_generation_elitism_preserves_best_through_filter() {
    // Fitness ranks "p" > "r" > "t", so ("p","q") is the top (elite) parent.
    const AggregateWeightedFitnessFunction fns =
        AggregateWeightedFitnessFunction({{[](const Specification& spec) {
                                               const std::string cond =
                                                   first_condition(spec);
                                               if (cond == "p") {
                                                   return 1.0;
                                               }
                                               if (cond == "r") {
                                                   return 0.5;
                                               }
                                               return 0.1;
                                           },
                                           1.0, ""}});
    const std::vector<ScoredSpecification> pop = score_population(
        Config{},
        {make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")}, fns);
    // Drop every offspring whose condition is "p": the elite's own offspring is
    // removed, so only elitism can keep a "p" specification alive.
    const std::vector<FilterFunction> filters = {
        make_predicate_filter("", [](const Specification& spec) {
            return first_condition(spec) != "p";
        })};
    Config cfg;
    cfg.crossover_rate = 0.0;
    cfg.mutation_rate = 0.0;

    const auto with_elitism =
        evolve_generation(cfg, pop, 3, 1, fns, filters, make_source({}, 0));
    const bool elite_survived =
        std::any_of(with_elitism.begin(), with_elitism.end(),
                    [](const ScoredSpecification& scored) {
                        return first_condition(scored.specification) == "p";
                    });
    expect(elite_survived,
           "evolve_generation: the top spec survives verbatim as an elite even "
           "when a filter removes its offspring");

    // With no elitism the same filter leaves no "p" specification behind.
    const auto without_elitism =
        evolve_generation(cfg, pop, 3, 0, fns, filters, make_source({}, 0));
    const bool elite_absent =
        std::none_of(without_elitism.begin(), without_elitism.end(),
                     [](const ScoredSpecification& scored) {
                         return first_condition(scored.specification) == "p";
                     });
    expect(elite_absent,
           "evolve_generation: without elitism the filtered-out top spec does "
           "not survive");
}

// --- evolve_generation under the NSGA-II scheme ---

// Two objectives over the specs "p"/"r"/"t": objective A ranks p > r > t and
// objective B ranks t > r > p, so "p" Pareto-dominates both "r" and "t" while
// "r" and "t" trade off (mutually non-dominating).
AggregateWeightedFitnessFunction two_objective_fns() {
    return AggregateWeightedFitnessFunction({{[](const Specification& spec) {
                                                  const std::string cond =
                                                      first_condition(spec);
                                                  if (cond == "p") {
                                                      return 1.0;
                                                  }
                                                  if (cond == "r") {
                                                      return 0.6;
                                                  }
                                                  return 0.2;
                                              },
                                              1.0, "a"},
                                             {[](const Specification& spec) {
                                                  const std::string cond =
                                                      first_condition(spec);
                                                  if (cond == "p") {
                                                      return 1.0;
                                                  }
                                                  if (cond == "r") {
                                                      return 0.4;
                                                  }
                                                  return 0.8;
                                              },
                                              1.0, "b"}});
}

Config nsga2_config() {
    Config cfg;
    cfg.selection_scheme = SelectionScheme::Nsga2;
    return cfg;
}

void test_evolve_generation_nsga2_produces_target_size() {
    const Config cfg = nsga2_config();
    const AggregateWeightedFitnessFunction fns = two_objective_fns();
    const std::vector<ScoredSpecification> pop = score_population(
        cfg, {make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")},
        fns);
    const auto next_gen =
        evolve_generation(cfg, pop, 2, 0, fns, {}, make_source({}, 0));
    expect(next_gen.size() == 2,
           "evolve_generation/nsga2: (mu+lambda) pooling still yields exactly "
           "target_size survivors");
}

void test_evolve_generation_nsga2_preserves_pareto_front_without_elitism() {
    Config cfg = nsga2_config();
    cfg.crossover_rate = 0.0;
    cfg.mutation_rate = 0.0;
    const AggregateWeightedFitnessFunction fns = two_objective_fns();
    const std::vector<ScoredSpecification> pop = score_population(
        cfg, {make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")},
        fns);
    // Remove every "p" offspring: only NSGA-II's (mu+lambda) pooling, which
    // retains the original parent, can keep the Pareto-optimal "p" alive with
    // elitism_size = 0.
    const std::vector<FilterFunction> filters = {
        make_predicate_filter("", [](const Specification& spec) {
            return first_condition(spec) != "p";
        })};
    const auto next_gen =
        evolve_generation(cfg, pop, 3, 0, fns, filters, make_source({}, 0));
    const bool p_survived =
        std::any_of(next_gen.begin(), next_gen.end(),
                    [](const ScoredSpecification& scored) {
                        return first_condition(scored.specification) == "p";
                    });
    expect(p_survived,
           "evolve_generation/nsga2: the Pareto-optimal parent survives via "
           "(mu+lambda) pooling even with no elitism and its offspring "
           "filtered out");
}

void test_evolve_generation_nsga2_is_deterministic() {
    const Config cfg = nsga2_config();
    const AggregateWeightedFitnessFunction fns = two_objective_fns();
    const std::vector<ScoredSpecification> pop = score_population(
        cfg, {make_spec("p", "q"), make_spec("r", "s"), make_spec("t", "u")},
        fns);
    const auto first =
        evolve_generation(cfg, pop, 2, 0, fns, {}, make_source({1, 2, 3}, 0));
    const auto second =
        evolve_generation(cfg, pop, 2, 0, fns, {}, make_source({1, 2, 3}, 0));
    bool identical = first.size() == second.size();
    for (std::size_t i = 0; identical && i < first.size(); ++i) {
        identical = first_condition(first[i].specification) ==
                    first_condition(second[i].specification);
    }
    expect(identical,
           "evolve_generation/nsga2: identical inputs and RNG produce an "
           "identical, stably-ordered generation");
}

void test_filters_for_generation_respects_intervals() {
    FilterFunction every = make_predicate_filter(
        "every", [](const Specification&) { return true; });
    every.set_interval(1);
    FilterFunction third = make_predicate_filter(
        "third", [](const Specification&) { return true; });
    third.set_interval(3);
    const std::vector<FilterFunction> filters = {every, third};

    const auto gen2 = filters_for_generation(filters, 2, false);
    expect(gen2.size() == 1 && gen2[0].name() == "every",
           "filters_for_generation: interval-3 filter should not run on gen 2");

    const auto gen3 = filters_for_generation(filters, 3, false);
    expect(gen3.size() == 2,
           "filters_for_generation: interval-3 filter should run on gen 3");
}

void test_filters_for_generation_last_runs_all_filters() {
    FilterFunction rare = make_predicate_filter(
        "rare", [](const Specification&) { return true; });
    rare.set_interval(100);
    const std::vector<FilterFunction> filters = {rare};

    expect(filters_for_generation(filters, 5, false).empty(),
           "filters_for_generation: a filter whose interval does not divide "
           "the generation should be skipped");
    expect(filters_for_generation(filters, 5, true).size() == 1,
           "filters_for_generation: the final generation should run every "
           "filter regardless of interval");
}

}  // namespace

void run_generation_tests() {
    test_score_population_single_function();
    test_score_population_weighted_aggregation();
    test_score_population_equal_weights_give_average();
    test_score_population_drops_failing_individual();
    test_score_population_circuit_breaker_trips();
    test_make_predicate_filter_keeps_matching();
    test_filter_population_empty_filter_list_keeps_all();
    test_filter_population_removes_failing();
    test_filter_population_applies_sequentially();
    test_filter_population_population_level_maximal_elements();
    test_evolve_generation_produces_target_size();
    test_evolve_generation_pads_up_to_target_size();
    test_evolve_generation_selects_parents_before_offspring_filtering();
    test_evolve_generation_elitism_preserves_best_through_filter();
    test_evolve_generation_nsga2_produces_target_size();
    test_evolve_generation_nsga2_preserves_pareto_front_without_elitism();
    test_evolve_generation_nsga2_is_deterministic();
    test_filters_for_generation_respects_intervals();
    test_filters_for_generation_last_runs_all_filters();
}
