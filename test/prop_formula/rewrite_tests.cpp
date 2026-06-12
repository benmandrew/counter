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

void test_simplify_idempotence() {
    Formula fml = Formula::make_binary(
        Formula::Kind::And, Formula::make_atom("A"), Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: A & A -> A");

    fml = Formula::make_binary(Formula::Kind::Or, Formula::make_atom("A"),
                               Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: A | A -> A");
}

void test_simplify_tautology() {
    Formula fml =
        Formula::make_binary(Formula::Kind::Implies, Formula::make_atom("A"),
                             Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: A -> A -> true");

    fml = Formula::make_binary(Formula::Kind::Iff, Formula::make_atom("A"),
                               Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: A <-> A -> true");
}

void test_simplify_excluded_middle() {
    Formula fml = Formula::make_binary(
        Formula::Kind::Or, Formula::make_atom("A"),
        Formula::make_unary(Formula::Kind::Not, Formula::make_atom("A")));
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: A | !A -> true");

    fml = Formula::make_binary(
        Formula::Kind::Or,
        Formula::make_unary(Formula::Kind::Not, Formula::make_atom("A")),
        Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: !A | A -> true");
}

void test_simplify_with_true() {
    const Formula tru;

    Formula fml =
        Formula::make_binary(Formula::Kind::And, Formula::make_atom("A"), tru);
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: A & true -> A");

    fml =
        Formula::make_binary(Formula::Kind::And, tru, Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: true & A -> A");

    fml = Formula::make_binary(Formula::Kind::Or, Formula::make_atom("A"), tru);
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: A | true -> true");

    fml = Formula::make_binary(Formula::Kind::Implies, tru,
                               Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: true -> A -> A");

    fml = Formula::make_binary(Formula::Kind::Implies, Formula::make_atom("A"),
                               tru);
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: A -> true -> true");

    fml =
        Formula::make_binary(Formula::Kind::Iff, Formula::make_atom("A"), tru);
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: A <-> true -> A");
}

void test_simplify_double_negation() {
    Formula fml = Formula::make_unary(
        Formula::Kind::Not,
        Formula::make_unary(Formula::Kind::Not, Formula::make_atom("A")));
    fml.simplify();
    expect(fml.to_string() == "A", "simplify: !!A -> A");
}

void test_simplify_compound() {
    // (A & A) -> A  =>  A -> A  =>  true
    Formula inner = Formula::make_binary(
        Formula::Kind::And, Formula::make_atom("A"), Formula::make_atom("A"));
    Formula fml = Formula::make_binary(Formula::Kind::Implies, inner,
                                       Formula::make_atom("A"));
    fml.simplify();
    expect(fml.to_string() == "true", "simplify: (A & A) -> A -> true");
}

}  // namespace

void run_prop_formula_rewrite_tests() {
    test_rewrite_post_order_identity();
    test_rewrite_post_order_rewrites_children_before_parent();
    test_simplify_idempotence();
    test_simplify_tautology();
    test_simplify_excluded_middle();
    test_simplify_with_true();
    test_simplify_double_negation();
    test_simplify_compound();
}
