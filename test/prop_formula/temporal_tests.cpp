#include <optional>
#include <string>

#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

Formula atom(const std::string& name) { return Formula::make_atom(name); }

// --- construction + to_string (SPOT syntax) ---

void test_unary_temporal_to_string() {
    expect(Formula::make_unary(Formula::Kind::Next, atom("p")).to_string() ==
               "X(p)",
           "temporal: Next renders as X(...)");
    expect(
        Formula::make_unary(Formula::Kind::Eventually, atom("p")).to_string() ==
            "F(p)",
        "temporal: Eventually renders as F(...)");
    expect(
        Formula::make_unary(Formula::Kind::Globally, atom("p")).to_string() ==
            "G(p)",
        "temporal: Globally renders as G(...)");
}

void test_binary_temporal_to_string() {
    expect(Formula::make_binary(Formula::Kind::Until, atom("p"), atom("q"))
                   .to_string() == "(p) U (q)",
           "temporal: Until renders as (l) U (r)");
    expect(Formula::make_binary(Formula::Kind::Release, atom("p"), atom("q"))
                   .to_string() == "(p) R (q)",
           "temporal: Release renders as (l) R (r)");
    expect(Formula::make_binary(Formula::Kind::WeakUntil, atom("p"), atom("q"))
                   .to_string() == "(p) W (q)",
           "temporal: WeakUntil renders as (l) W (r)");
}

void test_kind_reports_temporal() {
    expect(Formula::make_unary(Formula::Kind::Globally, atom("p")).kind() ==
               Formula::Kind::Globally,
           "temporal: kind() reports Globally");
    expect(Formula::make_binary(Formula::Kind::Until, atom("p"), atom("q"))
                   .kind() == Formula::Kind::Until,
           "temporal: kind() reports Until");
}

// --- nesting and mixed propositional/temporal ---

void test_globally_over_propositional() {
    const Formula inner =
        Formula::make_binary(Formula::Kind::And, atom("a"), atom("b"));
    const Formula glob = Formula::make_unary(Formula::Kind::Globally, inner);
    expect(glob.to_string() == "G((a) & (b))",
           "temporal: G over a propositional conjunction");
    expect(glob.n_subformulae() == inner.n_subformulae() + 1,
           "temporal: G adds exactly one node over its child");
}

void test_nested_temporal() {
    const Formula fml = Formula::make_unary(
        Formula::Kind::Globally,
        Formula::make_binary(
            Formula::Kind::Implies, atom("req"),
            Formula::make_unary(Formula::Kind::Eventually, atom("ack"))));
    expect(fml.to_string() == "G((req) -> (F(ack)))",
           "temporal: nested G(req -> F ack) request-response shape");
}

// --- is_propositional ---

void test_is_propositional() {
    expect(atom("p").is_propositional(),
           "temporal: bare atom is propositional");
    expect(Formula::make_binary(Formula::Kind::And, atom("a"), atom("b"))
               .is_propositional(),
           "temporal: conjunction is propositional");
    expect(
        !Formula::make_unary(Formula::Kind::Next, atom("p")).is_propositional(),
        "temporal: X(p) is not propositional");
    expect(!Formula::make_unary(
                Formula::Kind::Globally,
                Formula::make_binary(Formula::Kind::And, atom("a"), atom("b")))
                .is_propositional(),
           "temporal: G(a & b) is not propositional");
}

// --- extraction round-trips + arena consistency ---

void test_unary_child_extraction() {
    const Formula inner =
        Formula::make_binary(Formula::Kind::Or, atom("a"), atom("b"));
    const Formula glob = Formula::make_unary(Formula::Kind::Globally, inner);
    const auto child = glob.unary_child();
    expect(child.has_value(), "temporal: unary_child of G returns a child");
    expect(child.has_value() && child->to_string() == inner.to_string(),
           "temporal: extracted child string matches");
    // The extracted propositional subtree must be identical (hash + equality)
    // to the standalone one, so dedup / caching stay consistent.
    expect(child.has_value() && *child == inner,
           "temporal: extracted propositional subtree equals the original");
    expect(child.has_value() && child->hash() == inner.hash(),
           "temporal: extracted subtree hashes identically");
}

void test_binary_children_extraction() {
    const Formula left =
        Formula::make_binary(Formula::Kind::And, atom("a"), atom("b"));
    const Formula right = atom("c");
    const Formula unt = Formula::make_binary(Formula::Kind::Until, left, right);
    const auto children = unt.binary_children();
    expect(children.has_value(),
           "temporal: binary_children of U returns children");
    expect(children.has_value() && children->first == left &&
               children->second == right,
           "temporal: U children extract back to the operands");
}

void test_propositional_extraction_unchanged() {
    // Propositional binary_children still round-trips (regression guard for the
    // untouched string path).
    const Formula fml =
        Formula::make_binary(Formula::Kind::And, atom("p"), atom("q"));
    const auto children = fml.binary_children();
    expect(children.has_value() && children->first == atom("p") &&
               children->second == atom("q"),
           "temporal: propositional binary_children unaffected");
    expect(!Formula::make_unary(Formula::Kind::Globally, atom("p"))
                .binary_children()
                .has_value(),
           "temporal: binary_children of a unary temporal node is empty");
}

// --- similarity ---

void test_similarity_identical_temporal() {
    const Formula lhs = Formula::make_unary(Formula::Kind::Globally, atom("p"));
    const Formula rhs = Formula::make_unary(Formula::Kind::Globally, atom("p"));
    expect(lhs.syntactic_similarity(rhs) == 1.0,
           "temporal: identical temporal formulae are fully similar");
}

void test_similarity_distinguishes_operator() {
    const Formula glob =
        Formula::make_unary(Formula::Kind::Globally, atom("p"));
    const Formula fml =
        Formula::make_unary(Formula::Kind::Eventually, atom("p"));
    const double sim = glob.syntactic_similarity(fml);
    expect(sim > 0.0 && sim < 1.0,
           "temporal: G(p) vs F(p) share the atom but differ at the operator");
}

// --- rewrite over temporal preserves skeleton, rewrites prop subtrees ---

void test_rewrite_preserves_temporal_skeleton() {
    const Formula fml = Formula::make_unary(
        Formula::Kind::Globally,
        Formula::make_binary(Formula::Kind::And, atom("a"), atom("b")));
    const Formula rewritten = fml.rewrite_post_order(
        [](const Formula& subtree) -> std::optional<Formula> {
            if (subtree.atom_name() == "a") {
                return Formula::make_atom("z");
            }
            return std::nullopt;
        });
    expect(rewritten.to_string() == "G((z) & (b))",
           "temporal: rewrite replaces a propositional leaf under G, keeping "
           "the temporal skeleton");
}

}  // namespace

void run_prop_formula_temporal_tests() {
    test_unary_temporal_to_string();
    test_binary_temporal_to_string();
    test_kind_reports_temporal();
    test_globally_over_propositional();
    test_nested_temporal();
    test_is_propositional();
    test_unary_child_extraction();
    test_binary_children_extraction();
    test_propositional_extraction_unchanged();
    test_similarity_identical_temporal();
    test_similarity_distinguishes_operator();
    test_rewrite_preserves_temporal_skeleton();
}
