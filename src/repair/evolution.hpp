#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "config.hpp"
#include "dashboard.hpp"
#include "fitness/function.hpp"
#include "genetic/generation.hpp"
#include "genetic/random_source.hpp"
#include "reports.hpp"
#include "requirement.hpp"

// The FRETISH search, from the seed population to the specifications written
// out: everything between reading the input and reporting on the run.

// The objective names in registration order, used to label the per-objective
// means the dashboard reports. Read off the fitness function rather than a
// second hardcoded list, so a new objective needs no change here.
std::vector<std::string> fitness_objective_names(
    const AggregateWeightedFitnessFunction& fitness_function);

std::vector<ScoredSpecification> original_population(
    Specification& original_spec,
    const AggregateWeightedFitnessFunction& fitness_function,
    std::size_t population_size);

std::pair<std::vector<ScoredSpecification>, std::vector<FilterRunStats>>
run_evolution(const Config& cfg, std::vector<ScoredSpecification> population,
              const AggregateWeightedFitnessFunction& fitness_function,
              const std::vector<FilterFunction>& filter_functions,
              RandomSource& random_source, DashboardWriter& dashboard);

std::vector<Specification> collect_realizable_specifications(
    const std::vector<ScoredSpecification>& population);

// Applies the final screens to the realizable specifications: deduplication,
// then the weakening filter against @p original when run_weakening_filter is
// set, then the implication (maximality) filter when run_implication_filter is.
std::pair<std::vector<Specification>, std::vector<FilterRunStats>>
filter_maximal_specifications(const Config& cfg, const Specification& original,
                              const std::vector<Specification>& realizable_vec);

void write_specifications(
    const std::vector<ScoredSpecification>& scored,
    const AggregateWeightedFitnessFunction& fitness_function,
    const std::string& output_dir);
