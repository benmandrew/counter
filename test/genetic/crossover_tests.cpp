#include <utility>
#include <vector>

#include "genetic/crossover.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

RandomSource make_source(std::vector<bool> values, bool fallback) {
    return [values = std::move(values), fallback,
            index = std::size_t{0}]() mutable {
        if (index >= values.size()) {
            return fallback;
        }
        const bool value = values[index];
        ++index;
        return value;
    };
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
        first_parent, second_parent,
        make_source({false, false, true, true, true}, false));
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
        first_parent, second_parent,
        make_source({true, true, true, true, true, false, false}, false));
    expect(offspring.m_trigger.to_string() == "(P) & (R)",
           "crossover: trigger crossover should be able to combine atoms");
}

}  // namespace

void run_crossover_tests() {
    test_crossover_prefers_first_parent_with_false_source();
    test_timing_crossover_can_swap_parameters();
    test_formula_crossover_can_combine_atoms();
}
