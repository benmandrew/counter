#include "genetic/crossover.hpp"

#include <optional>
#include <stdexcept>
#include <type_traits>

namespace {

Formula::Kind pick_binary_kind(const RandomSource& random_bool) {
    const bool high_bit = random_bool();
    const bool low_bit = random_bool();
    const int selector = (high_bit ? 2 : 0) + (low_bit ? 1 : 0);
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
                          const RandomSource& random_bool) {
    switch (formula.kind()) {
        case Formula::Kind::Atom:
            return formula;
        case Formula::Kind::Not: {
            const Formula child = formula.unary_child().value();
            if (!random_bool()) {
                return formula;
            }
            return select_subformula(child, random_bool);
        }
        case Formula::Kind::And:
        case Formula::Kind::Or:
        case Formula::Kind::Implies:
        case Formula::Kind::Iff: {
            const auto children = formula.binary_children().value();
            if (!random_bool()) {
                return formula;
            }
            if (!random_bool()) {
                return select_subformula(children.first, random_bool);
            }
            return select_subformula(children.second, random_bool);
        }
    }
    throw std::logic_error("Failed to select a formula subformula.");
}

Formula replace_subformula(const Formula& formula, const Formula& donor,
                           const RandomSource& random_bool) {
    const Formula replacement = select_subformula(donor, random_bool);
    bool replaced = false;
    return formula.rewrite_post_order(
        [&](const Formula&) -> std::optional<Formula> {
            if (replaced || !random_bool()) {
                return std::nullopt;
            }
            replaced = true;
            return replacement;
        });
}

Formula combine_subformula(const Formula& formula, const Formula& donor,
                           const RandomSource& random_bool) {
    const Formula donor_subformula = select_subformula(donor, random_bool);
    bool combined = false;
    return formula.rewrite_post_order(
        [&](const Formula& subtree) -> std::optional<Formula> {
            if (combined || !random_bool()) {
                return std::nullopt;
            }
            combined = true;
            if (random_bool()) {
                return Formula::make_binary(pick_binary_kind(random_bool),
                                            subtree, donor_subformula);
            }
            return Formula::make_binary(pick_binary_kind(random_bool),
                                        donor_subformula, subtree);
        });
}

Formula crossover_formula(const Formula& first_parent,
                          const Formula& second_parent,
                          const RandomSource& random_bool) {
    if (!random_bool()) {
        return first_parent;
    }
    if (!random_bool()) {
        return second_parent;
    }
    if (!random_bool()) {
        return replace_subformula(first_parent, second_parent, random_bool);
    }
    return combine_subformula(first_parent, second_parent, random_bool);
}

template <typename TimingVariant>
Timing make_parameterized_timing(std::size_t ticks) {
    if constexpr (std::is_same_v<TimingVariant, timing::WithinTicks>) {
        return timing::within_ticks(ticks);
    } else {
        return timing::for_ticks(ticks);
    }
}

Timing crossover_timing(const Timing& first_parent, const Timing& second_parent,
                        const RandomSource& random_bool) {
    if (!random_bool) {
        throw std::invalid_argument("random_bool must be callable.");
    }

    return std::visit(
        [&](const auto& first_value) -> Timing {
            return std::visit(
                [&](const auto& second_value) -> Timing {
                    using First = std::decay_t<decltype(first_value)>;
                    using Second = std::decay_t<decltype(second_value)>;

                    if constexpr (std::is_same_v<First, Second>) {
                        if constexpr (std::is_same_v<First,
                                                     timing::WithinTicks> ||
                                      std::is_same_v<First, timing::ForTicks>) {
                            if (!random_bool()) {
                                return first_value;
                            }
                            if (!random_bool()) {
                                return second_value;
                            }
                            if (random_bool()) {
                                return make_parameterized_timing<First>(
                                    second_value.m_ticks);
                            }
                            return make_parameterized_timing<Second>(
                                first_value.m_ticks);
                        }
                        if (!random_bool()) {
                            return first_value;
                        }
                        return second_value;
                    } else {
                        if constexpr (
                            (std::is_same_v<First, timing::WithinTicks> ||
                             std::is_same_v<
                                 First,
                                 timing::
                                     ForTicks>)&&(std::
                                                      is_same_v<
                                                          Second,
                                                          timing::
                                                              WithinTicks> ||
                                                  std::is_same_v<
                                                      Second,
                                                      timing::ForTicks>)) {
                            if (!random_bool()) {
                                return first_value;
                            }
                            if (!random_bool()) {
                                return second_value;
                            }
                            if (random_bool()) {
                                return make_parameterized_timing<First>(
                                    second_value.m_ticks);
                            }
                            return make_parameterized_timing<Second>(
                                first_value.m_ticks);
                        } else {
                            if (!random_bool()) {
                                return first_value;
                            }
                            return second_value;
                        }
                    }
                },
                second_parent);
        },
        first_parent);
}

}  // namespace

Requirement crossover_requirements(const Requirement& first_parent,
                                   const Requirement& second_parent,
                                   const RandomSource& random_bool) {
    if (!random_bool) {
        throw std::invalid_argument("random_bool must be callable.");
    }
    Requirement offspring = first_parent;
    offspring.m_trigger = crossover_formula(
        first_parent.m_trigger, second_parent.m_trigger, random_bool);
    offspring.m_response = crossover_formula(
        first_parent.m_response, second_parent.m_response, random_bool);
    offspring.m_timing = crossover_timing(first_parent.m_timing,
                                          second_parent.m_timing, random_bool);
    return offspring;
}
