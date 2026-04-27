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
        first_parent, second_parent, make_source({}, false));
    expect(offspring.m_trigger.to_string() == "P",
           "crossover: false source should keep first parent's trigger");
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
    expect(offspring.m_trigger.to_string() == "(P) & (R)",
           "crossover: trigger crossover should be able to combine atoms");
}

}  // namespace

void run_crossover_tests() {
    test_crossover_prefers_first_parent_with_false_source();
    test_timing_crossover_can_swap_parameters();
    test_formula_crossover_can_combine_atoms();
}
