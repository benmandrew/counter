#include <utility>
#include <vector>

#include "genetic/crossover.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

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

void test_crossover_prefers_first_parent_with_false_source() {
    const Requirement first_parent{Formula("P"), Formula("Q"),
                                   timing::immediately()};
    const Requirement second_parent{Formula("R"), Formula("S"),
                                    timing::next_timepoint()};
    const Requirement offspring = crossover_requirements(
        first_parent, second_parent, make_source({}, 0U));
    expect(offspring.m_condition.to_string() == "P",
           "crossover: false source should keep first parent's condition");
    expect(offspring.m_response.to_string() == "Q",
           "crossover: false source should keep first parent's response");
    expect(std::holds_alternative<timing::Immediately>(offspring.m_timing),
           "crossover: false source should keep first parent's timing");
}

void test_timing_crossover_can_swap_parameters() {
    const Requirement first_parent{Formula("P"), Formula("Q"),
                                   timing::within_ticks(5)};
    const Requirement second_parent{Formula("P"), Formula("Q"),
                                    timing::for_ticks(10)};
    const Requirement offspring = crossover_requirements(
        first_parent, second_parent, make_source({0, 0, 2}, 0));
    const auto* within = std::get_if<timing::WithinTicks>(&offspring.m_timing);
    expect(within != nullptr,
           "crossover: parameter crossover should preserve the operator from"
           " the first parent when selected");
    expect(within->m_ticks == 10,
           "crossover: parameter crossover should be able to swap ticks");
}

void test_formula_crossover_can_combine_atoms() {
    const Requirement first_parent{Formula("P"), Formula("Q"),
                                   timing::immediately()};
    const Requirement second_parent{Formula("R"), Formula("S"),
                                    timing::next_timepoint()};
    const Requirement offspring = crossover_requirements(
        first_parent, second_parent, make_source({3, 1, 1, 0}, 0));
    expect(offspring.m_condition.to_string() == "(P) & (R)",
           "crossover: condition crossover should be able to combine atoms");
}

void test_crossover_keeps_non_weakenable_from_first_parent() {
    // guarantees[0] is locked in both parents, guarantees[1] is weakenable.
    // The locked position must be copied verbatim from the first parent (never
    // acting as, or receiving from, a crossover source), while the weakenable
    // position still crosses over with the same active source.
    const Requirement first_locked(Formula("a"), Formula("x"),
                                   timing::immediately(),
                                   ConditionType::Continual, false);
    const Requirement first_weak(Formula("P"), Formula("Q"),
                                 timing::immediately());
    const Requirement second_locked(Formula("b"), Formula("y"),
                                    timing::immediately(),
                                    ConditionType::Continual, false);
    const Requirement second_weak(Formula("R"), Formula("S"),
                                  timing::immediately());
    const Specification first_parent({}, {first_locked, first_weak}, {}, {});
    const Specification second_parent({}, {second_locked, second_weak}, {}, {});
    const Specification offspring = crossover_specifications(
        first_parent, second_parent, make_source({3, 1, 1, 0}, 0));
    expect(offspring.m_guarantees.size() == 2,
           "crossover: guarantee count should be preserved");
    expect(offspring.m_guarantees[0] == first_locked,
           "crossover: a non-weakenable requirement must be taken verbatim "
           "from the first parent");
    expect(offspring.m_guarantees[1].m_condition.to_string() == "(P) & (R)",
           "crossover: the weakenable requirement should still cross over");
}

}  // namespace

void run_crossover_tests() {
    test_crossover_prefers_first_parent_with_false_source();
    test_timing_crossover_can_swap_parameters();
    test_formula_crossover_can_combine_atoms();
    test_crossover_keeps_non_weakenable_from_first_parent();
}
