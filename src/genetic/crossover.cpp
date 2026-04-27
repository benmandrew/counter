#include "genetic/crossover.hpp"

#include <optional>
#include <stdexcept>
#include <type_traits>

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
    }
    throw std::logic_error("Failed to select a binary operator kind.");
}

Formula select_subformula(const Formula& formula,
                          const RandomSource& random_source) {
    switch (formula.kind()) {
        case Formula::Kind::Atom:
            return formula;
        case Formula::Kind::Not: {
            const Formula child = formula.unary_child().value();
            if (!random_source.next_bool()) {
                return formula;
            }
            return select_subformula(child, random_source);
        }
        case Formula::Kind::And:
        case Formula::Kind::Or:
        case Formula::Kind::Implies:
        case Formula::Kind::Iff: {
            const auto children = formula.binary_children().value();
            if (!random_source.next_bool()) {
                return formula;
            }
            if (!random_source.next_bool()) {
                return select_subformula(children.first, random_source);
            }
            return select_subformula(children.second, random_source);
        }
    }
    throw std::logic_error("Failed to select a formula subformula.");
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
    }
    throw std::logic_error("Failed to select formula crossover branch.");
}

template <typename TimingVariant>
Timing make_parameterized_timing(std::size_t ticks) {
    if constexpr (std::is_same_v<TimingVariant, timing::WithinTicks>) {
        return timing::within_ticks(ticks);
    } else {
        return timing::for_ticks(ticks);
    }
}

template <typename TimingVariant>
constexpr bool is_parameterized_timing_v =
    std::is_same_v<TimingVariant, timing::WithinTicks> ||
    std::is_same_v<TimingVariant, timing::ForTicks>;

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
    }
    throw std::logic_error("Failed to select timing crossover branch.");
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
    if (!random_source) {
        throw std::invalid_argument("random_source must be callable.");
    }
    Requirement offspring = first_parent;
    offspring.m_trigger = crossover_formula(
        first_parent.m_trigger, second_parent.m_trigger, random_source);
    offspring.m_response = crossover_formula(
        first_parent.m_response, second_parent.m_response, random_source);
    offspring.m_timing = crossover_timing(
        first_parent.m_timing, second_parent.m_timing, random_source);
    return offspring;
}
