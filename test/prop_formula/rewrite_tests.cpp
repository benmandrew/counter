#include <optional>
#include <string>

#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_rewrite_post_order_identity() {
    const Formula formula = Formula::make_binary(
        Formula::Kind::And, Formula::make_atom("P"), Formula::make_atom("Q"));
    const Formula rewritten = formula.rewrite_post_order(
        [](const Formula&) -> std::optional<Formula> { return std::nullopt; });
    expect(rewritten.to_string() == "(P) & (Q)",
           "prop-formula-rewrite: identity rewrite should preserve formula");
}

void test_rewrite_post_order_rewrites_children_before_parent() {
    const Formula formula = Formula::make_binary(
        Formula::Kind::And, Formula::make_atom("P"), Formula::make_atom("Q"));
    const Formula rewritten = formula.rewrite_post_order(
        [](const Formula& subtree) -> std::optional<Formula> {
            if (subtree.atom_name().has_value() &&
                subtree.atom_name().value() == "P") {
                return Formula::make_atom("R");
            }
            if (subtree.atom_name().has_value() &&
                subtree.atom_name().value() == "Q") {
                return Formula::make_atom("S");
            }
            if (subtree.to_string() == "(R) & (S)") {
                return Formula::make_atom("T");
            }
            return std::nullopt;
        });
    expect(
        rewritten.to_string() == "T",
        "prop-formula-rewrite: parent callback should see rewritten children");
}

}  // namespace

void run_prop_formula_rewrite_tests() {
    test_rewrite_post_order_identity();
    test_rewrite_post_order_rewrites_children_before_parent();
}
