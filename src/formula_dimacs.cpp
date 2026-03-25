#include "formula_dimacs.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum class NodeType {
    Variable,
    Not,
    And,
    Or,
    Implies,
    Iff,
};

struct Node {
    NodeType type;
    std::string variable;
    std::size_t left;
    std::size_t right;
};

class Parser {
   public:
    explicit Parser(std::string text) : text_(std::move(text)) {}

    std::vector<Node> parse() {
        std::vector<Node> nodes;
        const std::size_t root = parse_iff(nodes);
        skip_whitespace();
        if (!at_end()) {
            throw parse_error("Unexpected token at end of formula");
        }
        if (nodes.empty()) {
            throw std::invalid_argument("Formula must not be empty.");
        }
        if (root != nodes.size() - 1) {
            const Node root_node = nodes[root];
            nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(root));
            nodes.push_back(root_node);
        }

        return nodes;
    }

   private:
    std::size_t parse_iff(std::vector<Node>& nodes) {
        std::size_t lhs = parse_implies(nodes);
        while (try_consume("<->")) {
            const std::size_t rhs = parse_implies(nodes);
            lhs = push_binary(nodes, NodeType::Iff, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_implies(std::vector<Node>& nodes) {
        std::size_t lhs = parse_or(nodes);
        if (try_consume("->")) {
            const std::size_t rhs = parse_implies(nodes);
            lhs = push_binary(nodes, NodeType::Implies, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_or(std::vector<Node>& nodes) {
        std::size_t lhs = parse_and(nodes);
        while (try_consume("|")) {
            const std::size_t rhs = parse_and(nodes);
            lhs = push_binary(nodes, NodeType::Or, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_and(std::vector<Node>& nodes) {
        std::size_t lhs = parse_unary(nodes);
        while (try_consume("&")) {
            const std::size_t rhs = parse_unary(nodes);
            lhs = push_binary(nodes, NodeType::And, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_unary(std::vector<Node>& nodes) {
        if (try_consume("!") || try_consume("~")) {
            const std::size_t child = parse_unary(nodes);
            return push_unary(nodes, NodeType::Not, child);
        }
        if (try_consume("(")) {
            const std::size_t expression = parse_iff(nodes);
            if (!try_consume(")")) {
                throw parse_error("Expected ')' to close sub-expression");
            }
            return expression;
        }
        return parse_variable(nodes);
    }

    std::size_t parse_variable(std::vector<Node>& nodes) {
        skip_whitespace();
        if (at_end()) {
            throw parse_error("Expected variable, but reached end of formula");
        }
        const char first = text_[position_];
        if (!(std::isalpha(static_cast<unsigned char>(first)) ||
              first == '_')) {
            throw parse_error("Expected variable name");
        }
        std::string name;
        name.push_back(first);
        ++position_;
        while (!at_end()) {
            const char character = text_[position_];
            if (std::isalnum(static_cast<unsigned char>(character)) ||
                character == '_') {
                name.push_back(character);
                ++position_;
                continue;
            }
            break;
        }

        nodes.push_back(Node{NodeType::Variable, name, 0, 0});
        return nodes.size() - 1;
    }

    std::size_t push_unary(std::vector<Node>& nodes, NodeType type,
                           std::size_t child) {
        nodes.push_back(Node{type, "", child, 0});
        return nodes.size() - 1;
    }

    std::size_t push_binary(std::vector<Node>& nodes, NodeType type,
                            std::size_t lhs, std::size_t rhs) {
        nodes.push_back(Node{type, "", lhs, rhs});
        return nodes.size() - 1;
    }

    bool try_consume(const std::string& token) {
        skip_whitespace();
        if (text_.compare(position_, token.size(), token) == 0) {
            position_ += token.size();
            return true;
        }
        return false;
    }

    void skip_whitespace() {
        while (!at_end() &&
               std::isspace(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    }

    [[nodiscard]] bool at_end() const { return position_ >= text_.size(); }

    [[nodiscard]] std::invalid_argument parse_error(
        const std::string& message) const {
        std::ostringstream stream;
        stream << message << " at position " << position_ << ".";
        return std::invalid_argument(stream.str());
    }

    std::string text_;
    std::size_t position_ = 0;
};

class TseitinEncoder {
   public:
    explicit TseitinEncoder(const std::vector<Node>& nodes) : nodes_(nodes) {}

    DimacsCnf encode() {
        if (nodes_.empty()) {
            throw std::invalid_argument("Formula must not be empty.");
        }
        const int root_literal = encode_node(nodes_.size() - 1);
        clauses_.push_back({root_literal});
        return DimacsCnf{next_variable_id_ - 1, clauses_};
    }

   private:
    int encode_node(std::size_t index) {
        auto cache_it = node_literal_cache_.find(index);
        if (cache_it != node_literal_cache_.end()) {
            return cache_it->second;
        }
        const Node& node = nodes_[index];
        int literal = 0;
        switch (node.type) {
            case NodeType::Variable: {
                literal = get_or_create_symbol(node.variable);
                break;
            }
            case NodeType::Not: {
                const int child = encode_node(node.left);
                literal = new_auxiliary();
                add_clause({-literal, -child});
                add_clause({literal, child});
                break;
            }
            case NodeType::And: {
                const int left = encode_node(node.left);
                const int right = encode_node(node.right);
                literal = new_auxiliary();
                add_clause({-literal, left});
                add_clause({-literal, right});
                add_clause({literal, -left, -right});
                break;
            }
            case NodeType::Or: {
                const int left = encode_node(node.left);
                const int right = encode_node(node.right);
                literal = new_auxiliary();
                add_clause({literal, -left});
                add_clause({literal, -right});
                add_clause({-literal, left, right});
                break;
            }
            case NodeType::Implies: {
                const int left = encode_node(node.left);
                const int right = encode_node(node.right);
                literal = new_auxiliary();
                add_clause({-literal, -left, right});
                add_clause({left, literal});
                add_clause({-right, literal});
                break;
            }
            case NodeType::Iff: {
                const int left = encode_node(node.left);
                const int right = encode_node(node.right);
                literal = new_auxiliary();
                add_clause({-literal, -left, right});
                add_clause({-literal, left, -right});
                add_clause({literal, left, right});
                add_clause({literal, -left, -right});
                break;
            }
        }
        node_literal_cache_[index] = literal;
        return literal;
    }

    int get_or_create_symbol(const std::string& name) {
        auto it = symbol_to_variable_.find(name);
        if (it != symbol_to_variable_.end()) {
            return it->second;
        }

        const int variable = next_variable_id_;
        symbol_to_variable_[name] = variable;
        ++next_variable_id_;
        return variable;
    }

    int new_auxiliary() {
        const int variable = next_variable_id_;
        ++next_variable_id_;
        return variable;
    }

    void add_clause(std::vector<int> clause) {
        clauses_.push_back(std::move(clause));
    }

    const std::vector<Node>& nodes_;
    int next_variable_id_ = 1;
    std::unordered_map<std::string, int> symbol_to_variable_;
    std::unordered_map<std::size_t, int> node_literal_cache_;
    std::vector<std::vector<int>> clauses_;
};

}  // namespace

std::string DimacsCnf::to_dimacs() const {
    std::ostringstream stream;
    stream << "p cnf " << variable_count << ' ' << clauses.size() << "\n";
    for (const std::vector<int>& clause : clauses) {
        for (const int literal : clause) {
            stream << literal << ' ';
        }
        stream << "0\n";
    }
    return stream.str();
}

DimacsCnf formula_to_dimacs(const std::string& formula) {
    Parser parser(formula);
    const std::vector<Node> nodes = parser.parse();
    TseitinEncoder encoder(nodes);
    return encoder.encode();
}
