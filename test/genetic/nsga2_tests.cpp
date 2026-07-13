#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "genetic/nsga2.hpp"
#include "genetic/scored.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// --- dominates ---

void test_dominates_strict_improvement_in_all_objectives() {
    expect(dominates({0.6, 0.7}, {0.5, 0.5}),
           "dominates: better in every objective dominates");
}

void test_dominates_equal_in_one_better_in_other() {
    expect(dominates({0.5, 0.7}, {0.5, 0.5}),
           "dominates: equal in one, strictly better in the other, dominates");
}

void test_dominates_requires_strict_improvement_somewhere() {
    expect(!dominates({0.5, 0.5}, {0.5, 0.5}),
           "dominates: identical vectors do not dominate each other");
}

void test_dominates_mutually_incomparable() {
    expect(!dominates({0.6, 0.4}, {0.4, 0.6}),
           "dominates: a trade-off pair is mutually non-dominating (lhs)");
    expect(!dominates({0.4, 0.6}, {0.6, 0.4}),
           "dominates: a trade-off pair is mutually non-dominating (rhs)");
}

// --- non_domination_ranks ---

void test_ranks_layered_fronts() {
    // Front 0: the two trade-off extremes {0.9,0.1} and {0.1,0.9} plus the
    // balanced {0.5,0.5} — mutually non-dominating. {0.4,0.4} is dominated only
    // by {0.5,0.5} (front 1); {0.2,0.2} is dominated by both (front 2).
    const std::vector<std::vector<double>> objectives = {
        {0.9, 0.1}, {0.1, 0.9}, {0.5, 0.5}, {0.4, 0.4}, {0.2, 0.2}};
    const std::vector<std::size_t> ranks = non_domination_ranks(objectives);
    expect(ranks.size() == 5, "ranks: one entry per individual");
    expect(ranks[0] == 0 && ranks[1] == 0 && ranks[2] == 0,
           "ranks: mutually non-dominating solutions share front 0");
    expect(ranks[3] == 1, "ranks: {0.4,0.4} dominated only by {0.5,0.5}");
    expect(ranks[4] == 2, "ranks: {0.2,0.2} dominated across two fronts");
}

void test_ranks_total_order_chain() {
    const std::vector<std::vector<double>> objectives = {
        {0.1, 0.1}, {0.2, 0.2}, {0.3, 0.3}};
    const std::vector<std::size_t> ranks = non_domination_ranks(objectives);
    expect(ranks[2] == 0 && ranks[1] == 1 && ranks[0] == 2,
           "ranks: a fully ordered chain yields one solution per front");
}

void test_ranks_identical_vectors_share_front() {
    const std::vector<std::vector<double>> objectives = {
        {0.5, 0.5}, {0.5, 0.5}, {0.5, 0.5}};
    const std::vector<std::size_t> ranks = non_domination_ranks(objectives);
    expect(ranks[0] == 0 && ranks[1] == 0 && ranks[2] == 0,
           "ranks: identical (non-dominating) vectors all sit on front 0");
}

// --- crowding_distances ---

void test_crowding_boundaries_are_infinite() {
    const std::vector<std::vector<double>> objectives = {
        {0.0, 1.0}, {0.5, 0.5}, {1.0, 0.0}};
    const std::vector<std::size_t> ranks = {0, 0, 0};
    const std::vector<double> distances = crowding_distances(objectives, ranks);
    expect(std::isinf(distances[0]) && std::isinf(distances[2]),
           "crowding: the per-objective extremes get infinite distance");
    expect(std::isfinite(distances[1]),
           "crowding: an interior solution gets a finite distance");
    // Interior gap, normalised per objective: obj0 (1-0)/(1-0)=1, obj1
    // (1-0)/(1-0)=1 -> total 2.
    expect(distances[1] == 2.0,
           "crowding: interior distance is the sum of normalised neighbour "
           "gaps");
}

void test_crowding_denser_neighbour_has_smaller_distance() {
    // Four mutually non-dominating points along a trade-off front. Index 1
    // sits between two close neighbours (0.0 and 0.2 on obj0), while index 2
    // has a distant upper neighbour (1.0), so index 1 is the more crowded and
    // must receive the smaller distance. With only three points every interior
    // point's neighbours are the extremes, which is why four are needed here.
    const std::vector<std::vector<double>> objectives = {
        {0.0, 1.0}, {0.1, 0.9}, {0.2, 0.8}, {1.0, 0.0}};
    const std::vector<std::size_t> ranks = {0, 0, 0, 0};
    const std::vector<double> distances = crowding_distances(objectives, ranks);
    expect(std::isfinite(distances[1]) && std::isfinite(distances[2]),
           "crowding: both interior solutions get finite distances");
    expect(distances[1] < distances[2],
           "crowding: the interior solution with closer neighbours is more "
           "crowded and gets the smaller distance");
}

void test_crowding_zero_range_objective_contributes_nothing() {
    // obj1 is constant across the front; it must not produce NaN/inf for the
    // interior member, and obj0 alone drives the finite distance.
    const std::vector<std::vector<double>> objectives = {
        {0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
    const std::vector<std::size_t> ranks = {0, 0, 0};
    const std::vector<double> distances = crowding_distances(objectives, ranks);
    expect(std::isfinite(distances[1]) && distances[1] == 1.0,
           "crowding: a zero-range objective adds nothing and yields no NaN");
}

// --- nsga2_sort ---

Scored<int> make_scored(int value, std::vector<double> objectives) {
    Scored<int> scored;
    scored.specification = value;
    scored.objectives = std::move(objectives);
    return scored;
}

void test_nsga2_sort_orders_by_rank_then_crowding() {
    std::vector<Scored<int>> population;
    population.push_back(make_scored(0, {0.4, 0.4}));  // front 1
    population.push_back(make_scored(1, {0.9, 0.1}));  // front 0, boundary
    population.push_back(make_scored(2, {0.5, 0.5}));  // front 0, interior
    population.push_back(make_scored(3, {0.1, 0.9}));  // front 0, boundary
    nsga2_sort(population);
    expect(population.front().rank == 0,
           "nsga2_sort: front-0 solutions come first");
    expect(population.back().specification == 0 && population.back().rank == 1,
           "nsga2_sort: the dominated solution sorts last");
    // Within front 0 the two boundaries (infinite crowding) precede the
    // interior solution 2.
    expect(population[2].specification == 2,
           "nsga2_sort: the crowded interior solution sorts after the "
           "boundaries within its front");
}

void test_nsga2_sort_is_deterministic() {
    const auto build = []() {
        std::vector<Scored<int>> pop;
        pop.push_back(make_scored(0, {0.5, 0.5}));
        pop.push_back(make_scored(1, {0.5, 0.5}));
        pop.push_back(make_scored(2, {0.9, 0.1}));
        return pop;
    };
    std::vector<Scored<int>> first = build();
    std::vector<Scored<int>> second = build();
    nsga2_sort(first);
    nsga2_sort(second);
    for (std::size_t i = 0; i < first.size(); ++i) {
        expect(first[i].specification == second[i].specification,
               "nsga2_sort: identical input yields identical order (stable)");
    }
}

}  // namespace

void run_nsga2_tests() {
    test_dominates_strict_improvement_in_all_objectives();
    test_dominates_equal_in_one_better_in_other();
    test_dominates_requires_strict_improvement_somewhere();
    test_dominates_mutually_incomparable();
    test_ranks_layered_fronts();
    test_ranks_total_order_chain();
    test_ranks_identical_vectors_share_front();
    test_crowding_boundaries_are_infinite();
    test_crowding_denser_neighbour_has_smaller_distance();
    test_crowding_zero_range_objective_contributes_nothing();
    test_nsga2_sort_orders_by_rank_then_crowding();
    test_nsga2_sort_is_deterministic();
}
