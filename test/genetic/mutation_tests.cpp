#include <utility>
#include <vector>

#include "genetic/mutation.hpp"
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

void test_mutation_with_false_source_leaves_formula_unchanged() {
    const Formula formula("P & Q");
    // P & Q has 3 subformulae; fallback 1 gives next_index(3) = 1 != 0, so no
    // subformula is selected and the formula is left unchanged.
    const Formula mutated = mutate_formula(formula, {}, make_source({}, 1));
    expect(mutated.to_string() == "(P) & (Q)",
           "mutation: source that never selects a subformula should leave "
           "formula unchanged");
}

void test_mutation_renames_atom_to_one_from_atoms_list() {
    // True source forces the rename branch; atoms = {"Q"} so "P" becomes "Q".
    const Formula formula("P");
    const Formula mutated = mutate_formula(formula, {"Q"}, make_source({}, 1U));
    expect(mutated.to_string() == "Q",
           "mutation: true source should mutate atom to one from the provided "
           "atoms list");
}

void test_mutation_atom_unchanged_when_no_atoms_provided() {
    // True source forces the rename branch; empty atoms → name unchanged.
    const Formula formula("P");
    const Formula mutated = mutate_formula(formula, {}, make_source({}, 1U));
    expect(mutated.to_string() == "P",
           "mutation: atom name should be left unchanged when atoms list is "
           "empty");
}

void test_mutation_atom_selected_from_atoms_list() {
    // mutation_function consumes next_bool() = true (value 1),
    // mutate_atom_formula consumes next_bool() = true (value 1) → rename
    // branch, mutate_atom_name consumes next_index(3) = 2 → atoms[2] = "c".
    const Formula formula("x");
    const Formula mutated =
        mutate_formula(formula, {"a", "b", "c"}, make_source({1, 1, 2}, 0));
    expect(mutated.to_string() == "c",
           "mutation: atom should be replaced by the atom at the index chosen "
           "by the random source");
}

void test_timing_mutation_non_parameterized_becomes_within_one_tick() {
    const Timing mutated =
        mutate_timing(timing::next_timepoint(), make_source({}, 0U));
    const auto* within = std::get_if<timing::WithinTicks>(&mutated);
    expect(within != nullptr,
           "mutation: next-timepoint should weaken to within-ticks");
    expect(within->m_ticks == 1,
           "mutation: next-timepoint should weaken to within 1 tick");
}

void test_timing_mutation_immediately_becomes_within_one_tick() {
    const Timing mutated =
        mutate_timing(timing::immediately(), make_source({}, 0U));
    const auto* within = std::get_if<timing::WithinTicks>(&mutated);
    expect(within != nullptr,
           "mutation: immediately should weaken to within-ticks");
    expect(within->m_ticks == 1,
           "mutation: immediately should weaken to within 1 tick");
}

void test_timing_mutation_eventually_is_unchanged() {
    const Timing mutated =
        mutate_timing(timing::eventually(), make_source({}, 0U));
    expect(std::holds_alternative<timing::Eventually>(mutated),
           "mutation: eventually has no weakening and should be unchanged");
}

void test_timing_mutation_always_becomes_for_ticks() {
    const Timing mutated = mutate_timing(timing::always(), make_source({}, 0U));
    const auto* for_ticks = std::get_if<timing::ForTicks>(&mutated);
    expect(for_ticks != nullptr, "mutation: always should weaken to for-ticks");
    expect(for_ticks->m_ticks == 10,
           "mutation: always should weaken to for 10 ticks");
}

void test_timing_mutation_within_ticks_step_down() {
    // next_index(3) = 0 → step down: within_ticks(3 + 1 = 4)
    const Timing mutated =
        mutate_timing(timing::within_ticks(3), make_source({0}, 0));
    const auto* within = std::get_if<timing::WithinTicks>(&mutated);
    expect(within != nullptr,
           "mutation: within-ticks should remain within-ticks after step-down");
    expect(within->m_ticks == 4,
           "mutation: within-ticks step-down weakening should add one tick");
}

void test_timing_mutation_within_ticks_double() {
    // next_index(3) = 1 → double: within_ticks(3 * 2 = 6)
    const Timing mutated =
        mutate_timing(timing::within_ticks(3), make_source({1}, 0));
    const auto* within = std::get_if<timing::WithinTicks>(&mutated);
    expect(within != nullptr,
           "mutation: within-ticks should remain within-ticks after doubling");
    expect(within->m_ticks == 6,
           "mutation: within-ticks double weakening should double the count");
}

void test_timing_mutation_after_ticks_becomes_within_ticks() {
    const Timing mutated =
        mutate_timing(timing::after_ticks(3), make_source({}, 0));
    const auto* within = std::get_if<timing::WithinTicks>(&mutated);
    expect(within != nullptr,
           "mutation: after-ticks should weaken to within-ticks");
    expect(within->m_ticks == 4,
           "mutation: after 3 ticks should weaken to within 4 ticks");
}

}  // namespace

void run_mutation_tests() {
    test_mutation_with_false_source_leaves_formula_unchanged();
    test_mutation_renames_atom_to_one_from_atoms_list();
    test_mutation_atom_unchanged_when_no_atoms_provided();
    test_mutation_atom_selected_from_atoms_list();
    test_timing_mutation_non_parameterized_becomes_within_one_tick();
    test_timing_mutation_immediately_becomes_within_one_tick();
    test_timing_mutation_eventually_is_unchanged();
    test_timing_mutation_always_becomes_for_ticks();
    test_timing_mutation_within_ticks_step_down();
    test_timing_mutation_within_ticks_double();
    test_timing_mutation_after_ticks_becomes_within_ticks();
}
