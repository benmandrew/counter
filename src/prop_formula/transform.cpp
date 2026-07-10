#include <cassert>
#include <functional>
#include <memory>
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
        case NodeType::Next:
            return Formula::Kind::Next;
        case NodeType::Eventually:
            return Formula::Kind::Eventually;
        case NodeType::Globally:
            return Formula::Kind::Globally;
        case NodeType::Until:
            return Formula::Kind::Until;
        case NodeType::Release:
            return Formula::Kind::Release;
        case NodeType::WeakUntil:
            return Formula::Kind::WeakUntil;
    }
    assert(false);
    __builtin_unreachable();
}

NodeType kind_to_node_type(Formula::Kind kind) {
    switch (kind) {
        case Formula::Kind::Atom:
            return NodeType::Variable;
        case Formula::Kind::Not:
            return NodeType::Not;
        case Formula::Kind::And:
            return NodeType::And;
        case Formula::Kind::Or:
            return NodeType::Or;
        case Formula::Kind::Implies:
            return NodeType::Implies;
        case Formula::Kind::Iff:
            return NodeType::Iff;
        case Formula::Kind::Next:
            return NodeType::Next;
        case Formula::Kind::Eventually:
            return NodeType::Eventually;
        case Formula::Kind::Globally:
            return NodeType::Globally;
        case Formula::Kind::Until:
            return NodeType::Until;
        case Formula::Kind::Release:
            return NodeType::Release;
        case Formula::Kind::WeakUntil:
            return NodeType::WeakUntil;
    }
    assert(false);
    __builtin_unreachable();
}

// Appends a unary node of @p type over @p child's arena, producing a new arena
// with the root last. Child indices in the source arena are unchanged (the
// child keeps positions [0, child.size)); the new root points at the old root.
std::vector<Node> build_unary_arena(NodeType type,
                                    const std::vector<Node>& child) {
    assert(!child.empty());
    std::vector<Node> nodes = child;
    const std::size_t child_root = nodes.size() - 1;
    nodes.push_back(Node{type, "", child_root, 0});
    return nodes;
}

// Concatenates @p left and @p right arenas and appends a binary node of
// @p type. The right arena's internal child indices are shifted by the left
// arena's size; leaf (Variable) indices stay 0, matching how the parser lays
// out `(left) op (right)`.
std::vector<Node> build_binary_arena(NodeType type,
                                     const std::vector<Node>& left,
                                     const std::vector<Node>& right) {
    assert(!left.empty() && !right.empty());
    std::vector<Node> nodes = left;
    const std::size_t offset = nodes.size();
    const std::size_t left_root = offset - 1;
    for (Node node : right) {
        if (is_unary_node(node.m_type)) {
            node.m_left += offset;
        } else if (is_binary_node(node.m_type)) {
            node.m_left += offset;
            node.m_right += offset;
        }
        nodes.push_back(node);
    }
    const std::size_t right_root = nodes.size() - 1;
    nodes.push_back(Node{type, "", left_root, right_root});
    return nodes;
}

// Extracts the subtree rooted at @p root_index from @p nodes into a standalone
// arena (root last, post-order), remapping child indices. This reproduces the
// exact layout the parser would produce for the subtree's string form, so an
// extracted propositional subtree is byte-for-byte equal to the reparsed one.
std::vector<Node> extract_subtree(const std::vector<Node>& nodes,
                                  std::size_t root_index) {
    std::vector<Node> result;
    std::function<std::size_t(std::size_t)> visit =
        [&](std::size_t old_index) -> std::size_t {
        Node copy = nodes[old_index];
        if (is_unary_node(copy.m_type)) {
            copy.m_left = visit(nodes[old_index].m_left);
        } else if (is_binary_node(copy.m_type)) {
            const std::size_t new_left = visit(nodes[old_index].m_left);
            const std::size_t new_right = visit(nodes[old_index].m_right);
            copy.m_left = new_left;
            copy.m_right = new_right;
        } else {
            copy.m_left = 0;
            copy.m_right = 0;
        }
        result.push_back(copy);
        return result.size() - 1;
    };
    visit(root_index);
    return result;
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
            case NodeType::Next:
                return "X(" + to_string_recursive(node.m_left) + ")";
            case NodeType::Eventually:
                return "F(" + to_string_recursive(node.m_left) + ")";
            case NodeType::Globally:
                return "G(" + to_string_recursive(node.m_left) + ")";
            case NodeType::Until: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") U (" + right + ")";
            }
            case NodeType::Release: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") R (" + right + ")";
            }
            case NodeType::WeakUntil: {
                const std::string left = to_string_recursive(node.m_left);
                const std::string right = to_string_recursive(node.m_right);
                return "(" + left + ") W (" + right + ")";
            }
        }
        assert(false);
        __builtin_unreachable();
    };

    return to_string_recursive(index);
}

}  // namespace prop_formula_internal

Formula Formula::make_atom(const std::string& atom) { return Formula(atom); }

Formula Formula::from_node_arena(
    std::vector<prop_formula_internal::Node> nodes) {
    Formula result;
    result.m_impl = std::make_unique<Impl>(std::move(nodes));
    return result;
}

Formula Formula::make_unary(Kind kind, const Formula& child) {
    // Construction is done directly on the node arena rather than by building a
    // string and reparsing it. For propositional operators this yields an arena
    // byte-identical to the parser's (verified against the propositional test
    // suite); for temporal operators — and for any propositional operator whose
    // operand is itself temporal (e.g. !(X p)) — it is the only correct path,
    // since the propositional parser cannot read temporal syntax back.
    assert(kind == Kind::Not || kind == Kind::Next ||
           kind == Kind::Eventually || kind == Kind::Globally);
    return Formula::from_node_arena(prop_formula_internal::build_unary_arena(
        prop_formula_internal::kind_to_node_type(kind), child.m_impl->m_nodes));
}

Formula Formula::make_binary(Kind kind, const Formula& left,
                             const Formula& right) {
    switch (kind) {
        case Kind::And:
        case Kind::Or:
        case Kind::Implies:
        case Kind::Iff:
        case Kind::Until:
        case Kind::Release:
        case Kind::WeakUntil:
            return Formula::from_node_arena(
                prop_formula_internal::build_binary_arena(
                    prop_formula_internal::kind_to_node_type(kind),
                    left.m_impl->m_nodes, right.m_impl->m_nodes));
        case Kind::Atom:
        case Kind::Not:
        case Kind::Next:
        case Kind::Eventually:
        case Kind::Globally:
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
            // Temporal operators are left untouched by propositional
            // simplification; their subtrees are still simplified by the
            // post-order walk.
            case Kind::Next:
            case Kind::Eventually:
            case Kind::Globally:
            case Kind::Until:
            case Kind::Release:
            case Kind::WeakUntil:
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
    // Extraction is done on the node arena (not by reparsing a string), so it
    // works uniformly for propositional and temporal roots — including a
    // propositional operator whose child is temporal, e.g. the child of the
    // Not in !(X p). For a propositional subtree this yields an arena
    // byte-identical to the parser's.
    const prop_formula_internal::Node& root = m_impl->m_nodes.back();
    if (!prop_formula_internal::is_unary_node(root.m_type)) {
        return std::nullopt;
    }
    return from_node_arena(
        prop_formula_internal::extract_subtree(m_impl->m_nodes, root.m_left));
}

std::optional<std::pair<Formula, Formula>> Formula::binary_children() const {
    const prop_formula_internal::Node& root = m_impl->m_nodes.back();
    if (!prop_formula_internal::is_binary_node(root.m_type)) {
        return std::nullopt;
    }
    return std::make_pair(
        from_node_arena(prop_formula_internal::extract_subtree(m_impl->m_nodes,
                                                               root.m_left)),
        from_node_arena(prop_formula_internal::extract_subtree(m_impl->m_nodes,
                                                               root.m_right)));
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
            case prop_formula_internal::NodeType::Next:
            case prop_formula_internal::NodeType::Eventually:
            case prop_formula_internal::NodeType::Globally: {
                const Formula child = rewrite_subtree(node.m_left);
                rewritten_subtree = Formula::make_unary(
                    prop_formula_internal::node_type_to_kind(node.m_type),
                    child);
                break;
            }
            case prop_formula_internal::NodeType::Until:
            case prop_formula_internal::NodeType::Release:
            case prop_formula_internal::NodeType::WeakUntil: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree = Formula::make_binary(
                    prop_formula_internal::node_type_to_kind(node.m_type), left,
                    right);
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
