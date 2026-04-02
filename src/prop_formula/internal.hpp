#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "prop_formula.hpp"

namespace prop_formula_internal {

enum class NodeType {
    Variable,
    Not,
    And,
    Or,
    Implies,
    Iff,
};

struct Node {
    NodeType m_type;
    std::string m_variable;
    std::size_t m_left;
    std::size_t m_right;
};

struct DimacsCnf {
    int m_variable_count;
    std::vector<std::vector<int>> m_clauses;
};

std::vector<Node> parse_formula(const std::string& formula);
DimacsCnf encode_dimacs(const std::vector<Node>& nodes);
Formula::Kind node_type_to_kind(NodeType type);
std::string node_to_string(const std::vector<Node>& nodes, std::size_t index);

}  // namespace prop_formula_internal

struct Formula::Impl {
    std::vector<prop_formula_internal::Node> m_nodes;

    explicit Impl(const std::string& formula);
};
