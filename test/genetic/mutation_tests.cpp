#include <stdexcept>
#include <utility>
#include <vector>

#include "genetic/mutation.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

BooleanRandomSource make_source(std::vector<bool> values, bool fallback) {
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

void test_mutation_with_false_source_leaves_formula_unchanged() {
    const Formula formula("P & Q");
    const Formula mutated = mutate_formula(formula, make_source({}, false));
    expect(mutated.to_string() == "(P) & (Q)",
           "mutation: false source should leave formula unchanged");
}

void test_mutation_with_true_source_renames_atom() {
    const Formula formula("P");
    const Formula mutated = mutate_formula(formula, make_source({}, true));
    expect(
        mutated.to_string() == "P_mut",
        "mutation: true source should force atomic mutation to renamed atom");
}

void test_mutation_rejects_empty_random_source() {
    bool threw = false;
    try {
        (void)mutate_formula(Formula("P"), BooleanRandomSource{});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "mutation: empty boolean source should be rejected");
}

}  // namespace

void run_mutation_tests() {
    test_mutation_with_false_source_leaves_formula_unchanged();
    test_mutation_with_true_source_renames_atom();
    test_mutation_rejects_empty_random_source();
}
