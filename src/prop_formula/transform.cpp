#include <cassert>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "internal.hpp"

namespace prop_formula_internal {

Formula::Kind node_type_to_kind(NodeType type) {
    switch (type) {
        case NodeType::Variable:
            return Formula::Kind::Atom;
        case NodeType::Not:
            return Formula::Kind::Not;
        case NodeType::And:
            return Formula::Kind::And;
        case NodeType::Or:
            return Formula::Kind::Or;
        case NodeType::Implies:
            return Formula::Kind::Implies;
        case NodeType::Iff:
            return Formula::Kind::Iff;
    }
    assert(false);
    __builtin_unreachable();
}

std::string node_to_string(const std::vector<Node>& nodes, std::size_t index) {
    std::function<std::string(std::size_t)> to_string_recursive =
        [&nodes, &to_string_recursive](std::size_t node_index) -> std::string {
        const Node& node = nodes[node_index];
        switch (node.m_type) {
            case NodeType::Variable:
                return node.m_variable;
            case NodeType::Not: {
                const std::string child = to_string_recursive(node.m_left);
                return "!(" + child + ")";
            }
            case NodeType::And: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") & (" + right + ")";
            }
            case NodeType::Or: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") | (" + right + ")";
            }
            case NodeType::Implies: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") -> (" + right + ")";
            }
            case NodeType::Iff: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") <-> (" + right + ")";
            }
        }
        assert(false);
        __builtin_unreachable();
    };

    return to_string_recursive(index);
}

}  // namespace prop_formula_internal

Formula Formula::make_atom(const std::string& atom) { return Formula(atom); }

Formula Formula::make_unary([[maybe_unused]] Kind kind, const Formula& child) {
    assert(kind == Kind::Not);
    return Formula("!(" + child.to_string() + ")");
}

Formula Formula::make_binary(Kind kind, const Formula& left,
                             const Formula& right) {
    switch (kind) {
        case Kind::And:
            return Formula("(" + left.to_string() + ") & (" +
                           right.to_string() + ")");
        case Kind::Or:
            return Formula("(" + left.to_string() + ") | (" +
                           right.to_string() + ")");
        case Kind::Implies:
            return Formula("(" + left.to_string() + ") -> (" +
                           right.to_string() + ")");
        case Kind::Iff:
            return Formula("(" + left.to_string() + ") <-> (" +
                           right.to_string() + ")");
        case Kind::Atom:
        case Kind::Not:
            assert(false);
            __builtin_unreachable();
    }
    __builtin_unreachable();
}

void Formula::remove_double_negation() {
    auto remove_double_neg = [](const Formula& node) -> std::optional<Formula> {
        if (node.kind() == Formula::Kind::Not) {
            auto child = node.unary_child();
            if (child && child->kind() == Formula::Kind::Not) {
                auto grandchild = child->unary_child();
                if (grandchild) {
                    return grandchild;
                }
            }
        }
        return std::nullopt;
    };
    *this = this->rewrite_post_order(remove_double_neg);
}

namespace {

bool is_true_formula(const Formula& fml) {
    return fml.kind() == Formula::Kind::Atom && fml.atom_name() == "true";
}

bool is_false_formula(const Formula& fml) {
    return fml.kind() == Formula::Kind::Atom && fml.atom_name() == "false";
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
    return std::nullopt;
}

std::optional<Formula> simplify_or(const Formula& lhs, const Formula& rhs) {
    if (lhs == rhs) {
        return lhs;
    }
    if (is_true_formula(lhs) || is_true_formula(rhs)) {
        return Formula{};
    }
    if (rhs.kind() == Formula::Kind::Not) {
        const auto rch = rhs.unary_child();
        if (rch && *rch == lhs) {
            return Formula{};
        }
    }
    if (lhs.kind() == Formula::Kind::Not) {
        const auto lch = lhs.unary_child();
        if (lch && *lch == rhs) {
            return Formula{};
        }
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
    return std::nullopt;
}

}  // namespace

void Formula::simplify() {
    auto simplify_node = [](const Formula& node) -> std::optional<Formula> {
        if (node.kind() == Kind::Not) {
            return simplify_not(node);
        }
        const auto children = node.binary_children();
        if (!children) {
            return std::nullopt;
        }
        const Formula& lhs = children->first;
        const Formula& rhs = children->second;
        switch (node.kind()) {
            case Kind::And:
                return simplify_and(lhs, rhs);
            case Kind::Or:
                return simplify_or(lhs, rhs);
            case Kind::Implies:
                return simplify_implies(lhs, rhs);
            case Kind::Iff:
                return simplify_iff(lhs, rhs);
            case Kind::Atom:
            case Kind::Not:
                break;
        }
        return std::nullopt;
    };
    *this = this->rewrite_post_order(simplify_node);
}

Formula::Kind Formula::kind() const {
    assert(!m_impl->m_nodes.empty());
    return prop_formula_internal::node_type_to_kind(
        m_impl->m_nodes.back().m_type);
}

std::optional<std::string> Formula::atom_name() const {
    const prop_formula_internal::Node& root = m_impl->m_nodes.back();
    if (root.m_type != prop_formula_internal::NodeType::Variable) {
        return std::nullopt;
    }
    return root.m_variable;
}

std::optional<Formula> Formula::unary_child() const {
    const prop_formula_internal::Node& root = m_impl->m_nodes.back();
    if (root.m_type != prop_formula_internal::NodeType::Not) {
        return std::nullopt;
    }
    return Formula(
        prop_formula_internal::node_to_string(m_impl->m_nodes, root.m_left));
}

std::optional<std::pair<Formula, Formula>> Formula::binary_children() const {
    const prop_formula_internal::Node& root = m_impl->m_nodes.back();
    switch (root.m_type) {
        case prop_formula_internal::NodeType::And:
        case prop_formula_internal::NodeType::Or:
        case prop_formula_internal::NodeType::Implies:
        case prop_formula_internal::NodeType::Iff:
            return std::make_pair(Formula(prop_formula_internal::node_to_string(
                                      m_impl->m_nodes, root.m_left)),
                                  Formula(prop_formula_internal::node_to_string(
                                      m_impl->m_nodes, root.m_right)));
        case prop_formula_internal::NodeType::Variable:
        case prop_formula_internal::NodeType::Not:
            return std::nullopt;
    }
    __builtin_unreachable();
}

Formula Formula::rewrite_post_order(
    const RewriteCallback& rewrite_callback) const {
    if (!rewrite_callback) {
        return *this;
    }

    std::function<Formula(std::size_t)> rewrite_subtree =
        [this, &rewrite_subtree,
         &rewrite_callback](std::size_t index) -> Formula {
        const prop_formula_internal::Node& node = m_impl->m_nodes[index];
        Formula rewritten_subtree;
        switch (node.m_type) {
            case prop_formula_internal::NodeType::Variable:
                rewritten_subtree = Formula::make_atom(node.m_variable);
                break;
            case prop_formula_internal::NodeType::Not: {
                const Formula child = rewrite_subtree(node.m_left);
                rewritten_subtree = Formula::make_unary(Kind::Not, child);
                break;
            }
            case prop_formula_internal::NodeType::And: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree =
                    Formula::make_binary(Kind::And, left, right);
                break;
            }
            case prop_formula_internal::NodeType::Or: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree = Formula::make_binary(Kind::Or, left, right);
                break;
            }
            case prop_formula_internal::NodeType::Implies: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree =
                    Formula::make_binary(Kind::Implies, left, right);
                break;
            }
            case prop_formula_internal::NodeType::Iff: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree =
                    Formula::make_binary(Kind::Iff, left, right);
                break;
            }
        }

        if (const std::optional<Formula> replacement =
                rewrite_callback(rewritten_subtree);
            replacement.has_value()) {
            return *replacement;
        }
        return rewritten_subtree;
    };

    return rewrite_subtree(m_impl->m_nodes.size() - 1);
}

std::string Formula::to_string() const {
    if (m_impl->m_nodes.empty()) {
        return "";
    }

    return prop_formula_internal::node_to_string(m_impl->m_nodes,
                                                 m_impl->m_nodes.size() - 1);
}
