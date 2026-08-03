// Tests over the generation pipeline as an observable list of stages: that a
// consumer sees every stage without knowing which stages exist, that adding a
// filter adds a stage, and that the reported population sizes track the real
// ones. The draw stream the pipeline must preserve is pinned separately, in
// determinism_tests.cpp.

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "config.hpp"
#include "genetic/generation.hpp"
#include "genetic/pipeline.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

constexpr std::size_t k_target_size = 4;
constexpr std::size_t k_elitism_size = 1;

Specification make_spec(const std::string& condition,
                        const std::string& response) {
    return Specification({},
                         {Requirement{Formula(condition), Formula(response),
                                      timing::immediately()}},
                         {"a", "b"}, {"x", "y"});
}

RandomSource make_source() {
    return RandomSource(
        [](std::size_t upper_bound) { return std::size_t{0} % upper_bound; });
}

AggregateWeightedFitnessFunction constant_fitness() {
    return AggregateWeightedFitnessFunction(
        {{[](const Specification&) { return 0.5; }, 1.0, "constant"}});
}

std::vector<StageObservation> observe_generation(
    const std::vector<FilterFunction>& filters) {
    const Config cfg;
    const AggregateWeightedFitnessFunction fns = constant_fitness();
    const std::vector<ScoredSpecification> pop =
        score_population(cfg,
                         {make_spec("a", "x"), make_spec("b", "y"),
                          make_spec("a", "y"), make_spec("b", "x")},
                         fns);
    std::vector<StageObservation> seen;
    evolve_generation(
        cfg, pop, k_target_size, k_elitism_size, fns, filters, make_source(),
        nullptr, [&seen](const StageObservation& obs) { seen.push_back(obs); });
    return seen;
}

std::vector<std::string> stage_names(
    const std::vector<StageObservation>& observations) {
    std::vector<std::string> names;
    names.reserve(observations.size());
    for (const StageObservation& obs : observations) {
        names.push_back(obs.name);
    }
    return names;
}

bool contains(const std::vector<std::string>& names, const std::string& name) {
    return std::find(names.begin(), names.end(), name) != names.end();
}

std::size_t index_of(const std::vector<std::string>& names,
                     const std::string& name) {
    const auto found = std::find(names.begin(), names.end(), name);
    if (found == names.end()) {
        fail("pipeline: expected a stage named " + name);
    }
    return static_cast<std::size_t>(std::distance(names.begin(), found));
}

const StageObservation& stage(const std::vector<StageObservation>& observations,
                              const std::string& name) {
    return observations[index_of(stage_names(observations), name)];
}

void test_pipeline_reports_every_fixed_stage() {
    const std::vector<std::string> names = stage_names(observe_generation({}));
    // const char*, not const std::string&: the braced list holds string
    // literals, so a string reference would bind to a temporary built for each
    // one, which gcc rejects under -Wrange-loop-construct.
    for (const char* expected : {"order-parents", "breed", "filter-fallback",
                                 "restore-elites", "pad", "score", "select"}) {
        expect(contains(names, expected),
               std::string("pipeline: the observer should see the ") +
                   expected + " stage");
    }
    expect(names.size() == 7,
           "pipeline: a generation with no filters should report exactly the "
           "seven fixed stages");
}

void test_pipeline_stages_run_in_dependency_order() {
    const std::vector<std::string> names = stage_names(observe_generation({}));
    expect(index_of(names, "order-parents") < index_of(names, "breed"),
           "pipeline: parents must be ordered before breeding selects from "
           "them");
    expect(index_of(names, "restore-elites") < index_of(names, "pad"),
           "pipeline: elites must be restored before padding, so padding "
           "cannot duplicate its way past target_size first");
    expect(index_of(names, "pad") < index_of(names, "score"),
           "pipeline: the population must be padded before it is scored");
    expect(index_of(names, "score") < index_of(names, "select"),
           "pipeline: selection ranks scored individuals");
}

void test_pipeline_derives_one_stage_per_filter() {
    const std::vector<FilterFunction> filters = {
        make_predicate_filter("keep-all",
                              [](const Specification&) { return true; }),
        make_predicate_filter("also-keep-all",
                              [](const Specification&) { return true; })};
    const std::vector<std::string> names =
        stage_names(observe_generation(filters));
    expect(names.size() == 9,
           "pipeline: two filters should add two stages to the seven fixed "
           "ones, so a filter appears in the stage list without the pipeline "
           "being told about it");
    expect(contains(names, "keep-all") && contains(names, "also-keep-all"),
           "pipeline: each filter stage should carry its filter's name");
    expect(index_of(names, "breed") < index_of(names, "keep-all") &&
               index_of(names, "also-keep-all") <
                   index_of(names, "filter-fallback"),
           "pipeline: filter stages should sit between breeding and the "
           "fallback, in their registration order");
}

void test_pipeline_sizes_track_the_population() {
    const std::vector<StageObservation> seen = observe_generation({});
    // target_size 4 with elitism 1 breeds 3 offspring, restores 1 elite, and
    // pads back to 4.
    expect(stage(seen, "breed").n_out == 3,
           "pipeline: breeding should report the offspring count it produced");
    expect(stage(seen, "restore-elites").n_out == 4,
           "pipeline: restoring one elite should grow the population by one");
    expect(stage(seen, "pad").n_out == k_target_size,
           "pipeline: padding should report the target size");
    expect(stage(seen, "select").n_out == k_target_size,
           "pipeline: selection should report exactly target_size survivors");
}

void test_pipeline_filter_fallback_restores_rejected_offspring() {
    const std::vector<FilterFunction> filters = {make_predicate_filter(
        "reject-all", [](const Specification&) { return false; })};
    const std::vector<StageObservation> seen = observe_generation(filters);
    expect(stage(seen, "reject-all").n_out == 0,
           "pipeline: the filter should reject every offspring");
    expect(stage(seen, "filter-fallback").n_in == 0 &&
               stage(seen, "filter-fallback").n_out == 3,
           "pipeline: the fallback should restore the unfiltered offspring "
           "when the filter chain as a whole leaves nothing");
}

}  // namespace

void run_pipeline_tests() {
    test_pipeline_reports_every_fixed_stage();
    test_pipeline_stages_run_in_dependency_order();
    test_pipeline_derives_one_stage_per_filter();
    test_pipeline_sizes_track_the_population();
    test_pipeline_filter_fallback_restores_rejected_offspring();
}
