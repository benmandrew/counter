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
    const Formula mutated = mutate_formula(formula, {}, make_source({}, false));
    expect(mutated.to_string() == "(P) & (Q)",
           "mutation: false source should leave formula unchanged");
}

void test_mutation_renames_atom_to_one_from_atoms_list() {
    // True source forces the rename branch; atoms = {"Q"} so "P" becomes "Q".
    const Formula formula("P");
    const Formula mutated =
        mutate_formula(formula, {"Q"}, make_source({}, true));
    expect(mutated.to_string() == "Q",
           "mutation: true source should mutate atom to one from the provided "
           "atoms list");
}

void test_mutation_atom_unchanged_when_no_atoms_provided() {
    // True source forces the rename branch; empty atoms → name unchanged.
    const Formula formula("P");
    const Formula mutated = mutate_formula(formula, {}, make_source({}, true));
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

void test_timing_mutation_replaces_non_parameterized_timing() {
    const Timing mutated =
        mutate_timing(timing::next_timepoint(), make_source({}, false));
    expect(std::holds_alternative<timing::Immediately>(mutated),
           "mutation: next-timepoint should mutate to immediately on replace");
}

void test_timing_mutation_can_switch_to_parameterized_timing() {
    const Timing mutated =
        mutate_timing(timing::immediately(), make_source({1, 5, 0}, 0));
    const auto* for_ticks = std::get_if<timing::ForTicks>(&mutated);
    expect(for_ticks != nullptr,
           "mutation: non-parameterized timing should be able to become for");
    expect(for_ticks->m_ticks == 6,
           "mutation: parameterized timing should use generated tick count");
}

void test_timing_mutation_can_change_parameter_only() {
    const Timing mutated =
        mutate_timing(timing::within_ticks(3), make_source({1, 0, 1}, 1));
    const auto* within = std::get_if<timing::WithinTicks>(&mutated);
    expect(within != nullptr,
           "mutation: within timing should remain within for parameter-only "
           "mutation");
    expect(within->m_ticks != 3,
           "mutation: parameter-only mutation should change tick count");
}

}  // namespace

void run_mutation_tests() {
    test_mutation_with_false_source_leaves_formula_unchanged();
    test_mutation_renames_atom_to_one_from_atoms_list();
    test_mutation_atom_unchanged_when_no_atoms_provided();
    test_mutation_atom_selected_from_atoms_list();
    test_timing_mutation_replaces_non_parameterized_timing();
    test_timing_mutation_can_switch_to_parameterized_timing();
    test_timing_mutation_can_change_parameter_only();
}
