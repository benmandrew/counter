#include <stdexcept>
#include <string>

#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_make_atom_and_inspect() {
    const Formula formula = Formula::make_atom("P");
    expect(formula.kind() == Formula::Kind::Atom,
           "prop-formula-ast: atom should report Kind::Atom");
    expect(formula.atom_name().has_value(),
           "prop-formula-ast: atom should expose its name");
    expect(formula.atom_name().value() == "P",
           "prop-formula-ast: atom name should round-trip");
    expect(formula.to_string() == "P",
           "prop-formula-ast: atom should stringify to itself");
}

void test_make_unary_and_inspect() {
    const Formula child = Formula::make_atom("P");
    const Formula formula = Formula::make_unary(Formula::Kind::Not, child);
    expect(formula.kind() == Formula::Kind::Not,
           "prop-formula-ast: unary should report Kind::Not");
    expect(formula.unary_child().has_value(),
           "prop-formula-ast: unary should expose its child");
    expect(formula.unary_child()->to_string() == "P",
           "prop-formula-ast: unary child should round-trip");
    expect(formula.to_string() == "!(P)",
           "prop-formula-ast: unary should stringify canonically");
}

void test_make_binary_and_inspect() {
    const Formula left = Formula::make_atom("P");
    const Formula right = Formula::make_atom("Q");
    const Formula formula =
        Formula::make_binary(Formula::Kind::And, left, right);
    expect(formula.kind() == Formula::Kind::And,
           "prop-formula-ast: binary should report Kind::And");
    expect(formula.binary_children().has_value(),
           "prop-formula-ast: binary should expose its children");
    expect(formula.binary_children()->first.to_string() == "P",
           "prop-formula-ast: left child should round-trip");
    expect(formula.binary_children()->second.to_string() == "Q",
           "prop-formula-ast: right child should round-trip");
    expect(formula.to_string() == "(P) & (Q)",
           "prop-formula-ast: binary should stringify canonically");
}

void test_invalid_builder_kind_rejects() {
    bool unary_threw = false;
    try {
        (void)Formula::make_unary(Formula::Kind::And, Formula::make_atom("P"));
    } catch (const std::invalid_argument&) {
        unary_threw = true;
    }
    expect(unary_threw,
           "prop-formula-ast: make_unary should reject non-unary kinds");

    bool binary_threw = false;
    try {
        (void)Formula::make_binary(Formula::Kind::Not, Formula::make_atom("P"),
                                   Formula::make_atom("Q"));
    } catch (const std::invalid_argument&) {
        binary_threw = true;
    }
    expect(binary_threw,
           "prop-formula-ast: make_binary should reject non-binary kinds");
}

}  // namespace

void run_prop_formula_ast_tests() {
    test_make_atom_and_inspect();
    test_make_unary_and_inspect();
    test_make_binary_and_inspect();
    test_invalid_builder_kind_rejects();
}
