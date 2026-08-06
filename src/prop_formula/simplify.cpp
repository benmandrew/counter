#include <optional>
#include <string>

#include "prop_formula.hpp"

namespace {

bool is_true_formula(const Formula& fml) {
    return fml.kind() == Formula::Kind::Atom && fml.atom_name() == "true";
}

bool is_false_formula(const Formula& fml) {
    return fml.kind() == Formula::Kind::Atom && fml.atom_name() == "false";
}

// True when one of @p lhs / @p rhs is the syntactic negation of the other,
// i.e. they form a complementary pair phi / !phi.
bool is_negation_of(const Formula& lhs, const Formula& rhs) {
    if (lhs.kind() == Formula::Kind::Not) {
        const auto child = lhs.unary_child();
        if (child && *child == rhs) {
            return true;
        }
    }
    if (rhs.kind() == Formula::Kind::Not) {
        const auto child = rhs.unary_child();
        if (child && *child == lhs) {
            return true;
        }
    }
    return false;
}

// Negates @p fml, collapsing !!A -> A and folding the boolean constants, so
// that simplification never introduces a fresh double negation that a single
// post-order pass would leave behind.
Formula negate(const Formula& fml) {
    if (fml.kind() == Formula::Kind::Not) {
        if (const auto child = fml.unary_child()) {
            return *child;
        }
    }
    if (is_true_formula(fml)) {
        return Formula("false");
    }
    if (is_false_formula(fml)) {
        return Formula{};
    }
    return Formula::make_unary(Formula::Kind::Not, fml);
}

std::optional<Formula> simplify_not(const Formula& node) {
    const auto child = node.unary_child();
    if (!child) {
        return std::nullopt;
    }
    if (child->kind() == Formula::Kind::Not) {
        return child->unary_child();
    }
    if (is_true_formula(*child)) {
        return Formula("false");
    }
    if (is_false_formula(*child)) {
        return Formula{};
    }
    return std::nullopt;
}

std::optional<Formula> simplify_and(const Formula& lhs, const Formula& rhs) {
    if (lhs == rhs) {
        return lhs;
    }
    if (is_true_formula(lhs)) {
        return rhs;
    }
    if (is_true_formula(rhs)) {
        return lhs;
    }
    // Annihilation with false and contradiction A & !A both collapse to false.
    if (is_false_formula(lhs) || is_false_formula(rhs) ||
        is_negation_of(lhs, rhs)) {
        return Formula("false");
    }
    return std::nullopt;
}

std::optional<Formula> simplify_or(const Formula& lhs, const Formula& rhs) {
    if (lhs == rhs) {
        return lhs;
    }
    if (is_true_formula(lhs) || is_true_formula(rhs)) {
        return Formula{};
    }
    // Excluded middle A | !A collapses to true.
    if (is_negation_of(lhs, rhs)) {
        return Formula{};
    }
    // Identity with false: A | false -> A, false | A -> A.
    if (is_false_formula(rhs)) {
        return lhs;
    }
    if (is_false_formula(lhs)) {
        return rhs;
    }
    return std::nullopt;
}

std::optional<Formula> simplify_implies(const Formula& lhs,
                                        const Formula& rhs) {
    if (lhs == rhs) {
        return Formula{};
    }
    if (is_true_formula(rhs)) {
        return Formula{};
    }
    if (is_true_formula(lhs)) {
        return rhs;
    }
    // false -> A is vacuously true; A -> false is !A.
    if (is_false_formula(lhs)) {
        return Formula{};
    }
    if (is_false_formula(rhs)) {
        return negate(lhs);
    }
    return std::nullopt;
}

std::optional<Formula> simplify_iff(const Formula& lhs, const Formula& rhs) {
    if (lhs == rhs) {
        return Formula{};
    }
    if (is_true_formula(lhs)) {
        return rhs;
    }
    if (is_true_formula(rhs)) {
        return lhs;
    }
    // A <-> false is !A; complementary operands A <-> !A are contradictory.
    if (is_false_formula(lhs)) {
        return negate(rhs);
    }
    if (is_false_formula(rhs)) {
        return negate(lhs);
    }
    if (is_negation_of(lhs, rhs)) {
        return Formula("false");
    }
    return std::nullopt;
}

std::optional<Formula> simplify_node(const Formula& node) {
    if (node.kind() == Formula::Kind::Not) {
        return simplify_not(node);
    }
    const auto children = node.binary_children();
    if (!children) {
        return std::nullopt;
    }
    const Formula& lhs = children->first;
    const Formula& rhs = children->second;
    switch (node.kind()) {
        case Formula::Kind::And:
            return simplify_and(lhs, rhs);
        case Formula::Kind::Or:
            return simplify_or(lhs, rhs);
        case Formula::Kind::Implies:
            return simplify_implies(lhs, rhs);
        case Formula::Kind::Iff:
            return simplify_iff(lhs, rhs);
        case Formula::Kind::Atom:
        case Formula::Kind::Not:
        // Temporal operators are left untouched by propositional
        // simplification; their subtrees are still simplified by the
        // post-order walk.
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally:
        case Formula::Kind::Until:
        case Formula::Kind::Release:
        case Formula::Kind::WeakUntil:
            break;
    }
    return std::nullopt;
}

std::optional<Formula> strip_double_negation(const Formula& node) {
    if (node.kind() != Formula::Kind::Not) {
        return std::nullopt;
    }
    const auto child = node.unary_child();
    if (!child || child->kind() != Formula::Kind::Not) {
        return std::nullopt;
    }
    return child->unary_child();
}

}  // namespace

void Formula::remove_double_negation() {
    *this = this->rewrite_post_order(strip_double_negation);
}

void Formula::simplify() { *this = this->rewrite_post_order(simplify_node); }
