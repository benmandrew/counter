#include "genetic/crossover.hpp"

#include <cassert>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

Formula::Kind pick_binary_kind(const RandomSource& random_source) {
    const int selector = static_cast<int>(random_source.next_index(4));
    switch (selector) {
        case 0:
            return Formula::Kind::And;
        case 1:
            return Formula::Kind::Or;
        case 2:
            return Formula::Kind::Implies;
        case 3:
            return Formula::Kind::Iff;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

Formula select_subformula(const Formula& formula,
                          const RandomSource& random_source) {
    switch (formula.kind()) {
        case Formula::Kind::Atom:
            return formula;
        case Formula::Kind::Not: {
            const auto child_opt = formula.unary_child();
            if (!child_opt.has_value()) {
                assert(false);
                __builtin_unreachable();
            }
            const Formula& child = *child_opt;
            if (!random_source.next_bool()) {
                return formula;
            }
            return select_subformula(child, random_source);
        }
        case Formula::Kind::And:
        case Formula::Kind::Or:
        case Formula::Kind::Implies:
        case Formula::Kind::Iff: {
            const auto children_opt = formula.binary_children();
            if (!children_opt.has_value()) {
                assert(false);
                __builtin_unreachable();
            }
            const auto& children = *children_opt;
            if (!random_source.next_bool()) {
                return formula;
            }
            if (!random_source.next_bool()) {
                return select_subformula(children.first, random_source);
            }
            return select_subformula(children.second, random_source);
        }
    }
    assert(false);
    __builtin_unreachable();
}

Formula replace_subformula(const Formula& formula, const Formula& donor,
                           const RandomSource& random_source) {
    const Formula replacement = select_subformula(donor, random_source);
    bool replaced = false;
    return formula.rewrite_post_order(
        [&](const Formula&) -> std::optional<Formula> {
            if (replaced || !random_source.next_bool()) {
                return std::nullopt;
            }
            replaced = true;
            return replacement;
        });
}

Formula combine_subformula(const Formula& formula, const Formula& donor,
                           const RandomSource& random_source) {
    const Formula donor_subformula = select_subformula(donor, random_source);
    bool combined = false;
    return formula.rewrite_post_order(
        [&](const Formula& subtree) -> std::optional<Formula> {
            if (combined || !random_source.next_bool()) {
                return std::nullopt;
            }
            combined = true;
            if (random_source.next_bool()) {
                return Formula::make_binary(pick_binary_kind(random_source),
                                            subtree, donor_subformula);
            }
            return Formula::make_binary(pick_binary_kind(random_source),
                                        donor_subformula, subtree);
        });
}

Formula crossover_formula(const Formula& first_parent,
                          const Formula& second_parent,
                          const RandomSource& random_source) {
    const int selector = static_cast<int>(random_source.next_index(4));
    switch (selector) {
        case 0:
            return first_parent;
        case 1:
            return second_parent;
        case 2:
            return replace_subformula(first_parent, second_parent,
                                      random_source);
        case 3:
            return combine_subformula(first_parent, second_parent,
                                      random_source);
        default:
            assert(false);
            __builtin_unreachable();
    }
}

template <typename TimingVariant>
Timing make_parameterized_timing(std::size_t ticks) {
    if constexpr (std::is_same_v<TimingVariant, timing::WithinTicks>) {
        return timing::within_ticks(ticks);
    } else if constexpr (std::is_same_v<TimingVariant, timing::AfterTicks>) {
        return timing::after_ticks(ticks);
    } else {
        return timing::for_ticks(ticks);
    }
}

template <typename TimingVariant>
constexpr bool is_parameterized_timing_v =
    std::is_same_v<TimingVariant, timing::WithinTicks> ||
    std::is_same_v<TimingVariant, timing::ForTicks> ||
    std::is_same_v<TimingVariant, timing::AfterTicks>;

template <typename First, typename Second>
Timing crossover_parameterized_timing(const First& first_value,
                                      const Second& second_value,
                                      const RandomSource& random_source) {
    const int selector = static_cast<int>(random_source.next_index(4));
    switch (selector) {
        case 0:
            return first_value;
        case 1:
            return second_value;
        case 2:
            return make_parameterized_timing<First>(second_value.m_ticks);
        case 3:
            return make_parameterized_timing<Second>(first_value.m_ticks);
        default:
            assert(false);
            __builtin_unreachable();
    }
}

template <typename First, typename Second>
Timing crossover_timing_values(const First& first_value,
                               const Second& second_value,
                               const RandomSource& random_source) {
    if constexpr (std::is_same_v<First, Second>) {
        if constexpr (is_parameterized_timing_v<First>) {
            return crossover_parameterized_timing(first_value, second_value,
                                                  random_source);
        }
        if (!random_source.next_bool()) {
            return first_value;
        }
        return second_value;
    }
    if constexpr (is_parameterized_timing_v<First> &&
                  is_parameterized_timing_v<Second>) {
        return crossover_parameterized_timing(first_value, second_value,
                                              random_source);
    }
    if (!random_source.next_bool()) {
        return first_value;
    }
    return second_value;
}

Timing crossover_timing(const Timing& first_parent, const Timing& second_parent,
                        const RandomSource& random_source) {
    return std::visit(
        [&](const auto& first_value) -> Timing {
            return std::visit(
                [&](const auto& second_value) -> Timing {
                    return crossover_timing_values(first_value, second_value,
                                                   random_source);
                },
                second_parent);
        },
        first_parent);
}

}  // namespace

Requirement crossover_requirements(const Requirement& first_parent,
                                   const Requirement& second_parent,
                                   const RandomSource& random_source) {
    assert(random_source);
    Requirement offspring = first_parent;
    offspring.m_trigger = crossover_formula(
        first_parent.m_trigger, second_parent.m_trigger, random_source);
    offspring.m_response = crossover_formula(
        first_parent.m_response, second_parent.m_response, random_source);
    offspring.m_timing = crossover_timing(
        first_parent.m_timing, second_parent.m_timing, random_source);
    offspring.m_ltl = requirement_to_ltl(offspring);
    return offspring;
}

namespace {

std::vector<Requirement> crossover_req_lists(
    const std::vector<Requirement>& first,
    const std::vector<Requirement>& second, const RandomSource& random_source) {
    assert(first.size() == second.size());
    std::vector<Requirement> offspring;
    offspring.reserve(first.size());
    for (std::size_t i = 0; i < first.size(); ++i) {
        offspring.push_back(
            crossover_requirements(first[i], second[i], random_source));
    }
    return offspring;
}

}  // namespace

Specification crossover_specifications(const Specification& first_parent,
                                       const Specification& second_parent,
                                       const RandomSource& random_source) {
    assert(random_source);
    if (first_parent.m_assumptions.size() !=
            second_parent.m_assumptions.size() ||
        first_parent.m_guarantees.size() != second_parent.m_guarantees.size() ||
        first_parent.m_in_atoms != second_parent.m_in_atoms ||
        first_parent.m_out_atoms != second_parent.m_out_atoms) {
        return first_parent;
    }
    Specification offspring(
        crossover_req_lists(first_parent.m_assumptions,
                            second_parent.m_assumptions, random_source),
        crossover_req_lists(first_parent.m_guarantees,
                            second_parent.m_guarantees, random_source),
        first_parent.m_in_atoms, first_parent.m_out_atoms);
    // Specification constructor deduplicates; if dedup reduced the count the
    // offspring has a different structure than the parents and cannot safely
    // participate in future crossovers — fall back to first_parent.
    if (offspring.m_assumptions.size() != first_parent.m_assumptions.size() ||
        offspring.m_guarantees.size() != first_parent.m_guarantees.size()) {
        return first_parent;
    }
    return offspring;
}
