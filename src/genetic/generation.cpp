#include "genetic/generation.hpp"

#include <string>
#include <utility>
#include <vector>

#include "filter/bloat.hpp"
#include "filter/implication.hpp"

FilterFunction make_predicate_filter(
    std::string name, std::function<bool(const Specification&)> predicate) {
    return {std::move(name), [predicate = std::move(predicate)](
                                 const std::vector<Specification>& pop) {
                std::vector<Specification> survivors;
                survivors.reserve(pop.size());
                for (const Specification& spec : pop) {
                    if (predicate(spec)) {
                        survivors.push_back(spec);
                    }
                }
                return survivors;
            }};
}

Specification simplify_offspring(Specification offspring) {
    Specification pre_simplify = offspring;
    for (auto& req : offspring.m_assumptions) {
        if (!req.m_weakenable) {
            continue;
        }
        req.m_condition.simplify();
        req.m_response.simplify();
        req.m_ltl = requirement_to_ltl(req);
    }
    for (auto& req : offspring.m_guarantees) {
        if (!req.m_weakenable) {
            continue;
        }
        req.m_condition.simplify();
        req.m_response.simplify();
        req.m_ltl = requirement_to_ltl(req);
    }
    Specification rededuped(offspring.m_assumptions, offspring.m_guarantees,
                            offspring.m_in_atoms, offspring.m_out_atoms);
    if (rededuped.m_assumptions.size() != pre_simplify.m_assumptions.size() ||
        rededuped.m_guarantees.size() != pre_simplify.m_guarantees.size()) {
        return pre_simplify;
    }
    return rededuped;
}

const GeneticOperators<Specification>& fretish_operators() {
    static const GeneticOperators<Specification> ops{
        [](const Specification& first, const Specification& second,
           const RandomSource& random_source) {
            return crossover_specifications(first, second, random_source);
        },
        [](const Specification& spec, const RandomSource& random_source,
           const Config& cfg) {
            return mutate_specification(spec, random_source, cfg);
        },
        [](Specification spec) { return simplify_offspring(std::move(spec)); }};
    return ops;
}

std::vector<ScoredSpecification> evolve_generation(
    const Config& cfg, const std::vector<ScoredSpecification>& population,
    std::size_t target_size, std::size_t elitism_size,
    const AggregateWeightedFitnessFunction& fitness_functions,
    const std::vector<FilterFunction>& filter_functions,
    const RandomSource& random_source,
    const GenerationProgressCallback& on_progress) {
    return evolve_generation_generic<Specification>(
        cfg, population, target_size, elitism_size, fitness_functions,
        filter_functions, fretish_operators(), random_source, on_progress);
}

std::vector<FilterFunction> get_filter_functions(
    const Config& cfg, Specification original, SatisfiabilityChecker& checker) {
    std::vector<FilterFunction> filters;
    FilterFunction dedup = make_dedup_filter();
    dedup.set_interval(cfg.dedup_filter_interval);
    filters.push_back(std::move(dedup));
    FilterFunction bloat = make_bloat_cap_filter(original);
    bloat.set_interval(cfg.bloat_filter_interval);
    filters.push_back(std::move(bloat));
    // A false condition is vacuously satisfied by every trace, so it
    // imposes no constraint; forbid it from surviving as a breeding
    // candidate rather than letting the fitness function alone
    // discourage it.
    FilterFunction false_condition =
        make_predicate_filter("false-condition", [](const Specification& spec) {
            return !specification_has_false_condition(spec);
        });
    false_condition.set_interval(cfg.false_condition_filter_interval);
    filters.push_back(std::move(false_condition));
    if (cfg.run_weakening_filter) {
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
        FilterFunction weakening =
            make_weakening_filter(std::move(original), checker);
        weakening.set_interval(cfg.weakening_filter_interval);
        filters.push_back(std::move(weakening));
    }
    return filters;
}

std::vector<FilterFunction> filters_for_generation(
    const std::vector<FilterFunction>& filters, std::size_t generation,
    bool is_last_generation) {
    std::vector<FilterFunction> active;
    active.reserve(filters.size());
    for (const FilterFunction& filter : filters) {
        if (is_last_generation || generation % filter.interval() == 0) {
            active.push_back(filter);
        }
    }
    return active;
}

std::vector<FilterFunction> get_final_filter_functions(
    const Config& cfg, SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_impl_progress) {
    std::vector<FilterFunction> filters;
    filters.push_back(make_dedup_filter());
    if (cfg.run_implication_filter) {
        filters.push_back(make_implication_filter(checker, on_impl_progress));
    }
    return filters;
}
