#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "config.hpp"
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

void test_timing_mutation_always_is_unchanged() {
    const Timing mutated = mutate_timing(timing::always(), make_source({}, 0U));
    expect(std::holds_alternative<timing::Always>(mutated),
           "mutation: always must not be weakened and should be unchanged");
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

void test_mutation_all_locked_is_noop() {
    const Specification spec(
        {},
        {Requirement(Formula("a"), Formula("b"), timing::immediately(),
                     ConditionType::Continual, false)},
        {"a"}, {"b"});
    const Config cfg;
    const Specification result =
        mutate_specification(spec, make_source({}, 0), cfg);
    expect(result == spec,
           "mutation: a spec whose only requirement is non-weakenable is "
           "returned unchanged");
}

void test_mutation_skips_non_weakenable_requirement() {
    // guarantees[0] is locked, guarantees[1] is weakenable. Only index 1 is
    // eligible, so the forced timing mutation must land on the weakenable
    // requirement and leave the locked one untouched.
    const Requirement locked(Formula("a"), Formula("b"), timing::immediately(),
                             ConditionType::Continual, false);
    const Requirement weak(Formula("c"), Formula("d"), timing::immediately(),
                           ConditionType::Continual, true);
    const Specification spec({}, {locked, weak}, {"a", "c"}, {"b", "d"});
    Config cfg;
    cfg.p_response = 0.0;
    cfg.p_trigger = 0.0;
    cfg.p_timing = 1.0;
    const Specification result =
        mutate_specification(spec, make_source({}, 0), cfg);
    expect(result.m_guarantees.size() == 2,
           "mutation: guarantee count should be preserved");
    expect(std::holds_alternative<timing::Immediately>(
               result.m_guarantees[0].m_timing) &&
               !result.m_guarantees[0].m_weakenable,
           "mutation: the non-weakenable requirement must be left untouched");
    const auto* within =
        std::get_if<timing::WithinTicks>(&result.m_guarantees[1].m_timing);
    expect(within != nullptr && within->m_ticks == 1,
           "mutation: the weakenable requirement must be the one mutated");
}

void test_condition_mutation_never_introduces_output_atom() {
    // Inputs and outputs are disjoint and distinctly named. With p_trigger = 1
    // every mutation rewrites the trigger; across many seeds this exercises
    // both atom renaming and new-atom introduction, none of which may pull an
    // output atom ("B"/"D") into the condition.
    const Requirement guar(Formula("a & c"), Formula("B"), timing::always(),
                           ConditionType::Trigger, true);
    const Specification spec({}, {guar}, {"a", "c"}, {"B", "D"});
    Config cfg;
    cfg.p_response = 0.0;
    cfg.p_trigger = 1.0;
    cfg.p_timing = 0.0;
    for (std::size_t seed = 0; seed < 200; ++seed) {
        const Specification result =
            mutate_specification(spec, make_random_source_from_seed(seed), cfg);
        const std::string condition =
            result.m_guarantees[0].m_condition.to_string();
        expect(condition.find('B') == std::string::npos &&
                   condition.find('D') == std::string::npos,
               "mutation: trigger mutation must never introduce an output atom "
               "into a condition");
    }
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
    test_timing_mutation_always_is_unchanged();
    test_timing_mutation_within_ticks_step_down();
    test_timing_mutation_within_ticks_double();
    test_timing_mutation_after_ticks_becomes_within_ticks();
    test_mutation_all_locked_is_noop();
    test_mutation_skips_non_weakenable_requirement();
    test_condition_mutation_never_introduces_output_atom();
}
