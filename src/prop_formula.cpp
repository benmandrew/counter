#include "prop_formula.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
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
    NodeType m_type;
    std::string m_variable;
    std::size_t m_left;
    std::size_t m_right;
};

class Parser {
   private:
    std::string m_text;
    std::size_t m_position = 0;
    std::vector<Node> m_nodes;

   public:
    explicit Parser(std::string text) : m_text(std::move(text)) {}

    std::vector<Node> parse() {
        m_position = 0;
        m_nodes.clear();
        const std::size_t root = parse_iff();
        skip_whitespace();
        if (!at_end()) {
            throw parse_error("Unexpected token at end of formula");
        }
        if (m_nodes.empty()) {
            throw std::invalid_argument("Formula must not be empty.");
        }
        if (root != m_nodes.size() - 1) {
            const Node root_node = m_nodes[root];
            m_nodes.erase(m_nodes.begin() + static_cast<std::ptrdiff_t>(root));
            m_nodes.push_back(root_node);
        }

        return m_nodes;
    }

   private:
    std::size_t parse_iff() {
        std::size_t lhs = parse_implies();
        while (try_consume("<->")) {
            const std::size_t rhs = parse_implies();
            lhs = push_binary(NodeType::Iff, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_implies() {
        std::size_t lhs = parse_or();
        if (try_consume("->")) {
            const std::size_t rhs = parse_implies();
            lhs = push_binary(NodeType::Implies, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_or() {
        std::size_t lhs = parse_and();
        while (try_consume("|")) {
            const std::size_t rhs = parse_and();
            lhs = push_binary(NodeType::Or, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_and() {
        std::size_t lhs = parse_unary();
        while (try_consume("&")) {
            const std::size_t rhs = parse_unary();
            lhs = push_binary(NodeType::And, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_unary() {
        if (try_consume("!") || try_consume("~")) {
            const std::size_t child = parse_unary();
            return push_unary(NodeType::Not, child);
        }
        if (try_consume("(")) {
            const std::size_t expression = parse_iff();
            if (!try_consume(")")) {
                throw parse_error("Expected ')' to close sub-expression");
            }
            return expression;
        }
        return parse_variable();
    }

    std::size_t parse_variable() {
        skip_whitespace();
        if (at_end()) {
            throw parse_error("Expected variable, but reached end of formula");
        }
        const char first = m_text[m_position];
        if (!(std::isalpha(static_cast<unsigned char>(first)) ||
              first == '_')) {
            throw parse_error("Expected variable name");
        }
        std::string name;
        name.push_back(first);
        ++m_position;
        while (!at_end()) {
            const char character = m_text[m_position];
            if (std::isalnum(static_cast<unsigned char>(character)) ||
                character == '_') {
                name.push_back(character);
                ++m_position;
                continue;
            }
            break;
        }

        m_nodes.push_back(Node{NodeType::Variable, name, 0, 0});
        return m_nodes.size() - 1;
    }

    std::size_t push_unary(NodeType type, std::size_t child) {
        m_nodes.push_back(Node{type, "", child, 0});
        return m_nodes.size() - 1;
    }

    std::size_t push_binary(NodeType type, std::size_t lhs, std::size_t rhs) {
        m_nodes.push_back(Node{type, "", lhs, rhs});
        return m_nodes.size() - 1;
    }

    bool try_consume(const std::string& token) {
        skip_whitespace();
        if (m_text.compare(m_position, token.size(), token) == 0) {
            m_position += token.size();
            return true;
        }
        return false;
    }

    void skip_whitespace() {
        while (!at_end() &&
               std::isspace(static_cast<unsigned char>(m_text[m_position]))) {
            ++m_position;
        }
    }

    [[nodiscard]] bool at_end() const { return m_position >= m_text.size(); }

    [[nodiscard]] std::invalid_argument parse_error(
        const std::string& message) const {
        std::ostringstream stream;
        stream << message << " at position " << m_position << ".";
        return std::invalid_argument(stream.str());
    }
};

struct DimacsCnf {
    int m_variable_count;
    std::vector<std::vector<int>> m_clauses;
};

class TseitinEncoder {
   private:
    const std::vector<Node>& m_nodes;
    int m_next_variable_id = 1;
    std::unordered_map<std::string, int> m_symbol_to_variable;
    std::unordered_map<std::size_t, int> m_node_literal_cache;
    std::vector<std::vector<int>> m_clauses;

   public:
    explicit TseitinEncoder(const std::vector<Node>& nodes) : m_nodes(nodes) {}

    DimacsCnf encode() {
        if (m_nodes.empty()) {
            throw std::invalid_argument("Formula must not be empty.");
        }
        const int root_literal =
            encode_node(static_cast<std::size_t>(m_nodes.size() - 1));
        m_clauses.push_back({root_literal});
        return DimacsCnf{m_next_variable_id - 1, m_clauses};
    }

   private:
    int encode_node(std::size_t index) {
        auto cache_it = m_node_literal_cache.find(index);
        if (cache_it != m_node_literal_cache.end()) {
            return cache_it->second;
        }
        const Node& node = m_nodes[index];
        int literal = 0;
        switch (node.m_type) {
            case NodeType::Variable: {
                literal = get_or_create_symbol(node.m_variable);
                break;
            }
            case NodeType::Not: {
                const int child = encode_node(node.m_left);
                literal = new_auxiliary();
                add_clause({-literal, -child});
                add_clause({literal, child});
                break;
            }
            case NodeType::And: {
                const int left = encode_node(node.m_left);
                const int right = encode_node(node.m_right);
                literal = new_auxiliary();
                add_clause({-literal, left});
                add_clause({-literal, right});
                add_clause({literal, -left, -right});
                break;
            }
            case NodeType::Or: {
                const int left = encode_node(node.m_left);
                const int right = encode_node(node.m_right);
                literal = new_auxiliary();
                add_clause({literal, -left});
                add_clause({literal, -right});
                add_clause({-literal, left, right});
                break;
            }
            case NodeType::Implies: {
                const int left = encode_node(node.m_left);
                const int right = encode_node(node.m_right);
                literal = new_auxiliary();
                add_clause({-literal, -left, right});
                add_clause({left, literal});
                add_clause({-right, literal});
                break;
            }
            case NodeType::Iff: {
                const int left = encode_node(node.m_left);
                const int right = encode_node(node.m_right);
                literal = new_auxiliary();
                add_clause({-literal, -left, right});
                add_clause({-literal, left, -right});
                add_clause({literal, left, right});
                add_clause({literal, -left, -right});
                break;
            }
        }
        m_node_literal_cache[index] = literal;
        return literal;
    }

    int get_or_create_symbol(const std::string& name) {
        auto it = m_symbol_to_variable.find(name);
        if (it != m_symbol_to_variable.end()) {
            return it->second;
        }

        const int variable = m_next_variable_id;
        m_symbol_to_variable[name] = variable;
        ++m_next_variable_id;
        return variable;
    }

    int new_auxiliary() {
        const int variable = m_next_variable_id;
        ++m_next_variable_id;
        return variable;
    }

    void add_clause(std::vector<int> clause) {
        m_clauses.push_back(std::move(clause));
    }
};

}  // namespace

struct Formula::Impl {
    std::vector<Node> m_nodes;

    explicit Impl(const std::string& formula) {
        Parser parser(formula);
        m_nodes = parser.parse();
    }
};

Formula::Formula(const std::string& formula)
    : m_impl(std::make_unique<Impl>(formula)) {}

Formula::Formula(const Formula& other)
    : m_impl(std::make_unique<Impl>(*other.m_impl)) {}

Formula::Formula(Formula&& other) noexcept = default;

Formula& Formula::operator=(const Formula& other) {
    if (this != &other) {
        *m_impl = *other.m_impl;
    }
    return *this;
}

Formula& Formula::operator=(Formula&& other) noexcept = default;

Formula::~Formula() = default;

std::string Formula::to_dimacs() const {
    TseitinEncoder encoder(m_impl->m_nodes);
    DimacsCnf cnf = encoder.encode();
    std::ostringstream stream;
    stream << "p cnf " << cnf.m_variable_count << ' ' << cnf.m_clauses.size()
           << "\n";
    for (const std::vector<int>& clause : cnf.m_clauses) {
        for (const int literal : clause) {
            stream << literal << ' ';
        }
        stream << "0\n";
    }
    return stream.str();
}

double Formula::syntactic_similarity(const Formula& other) const {
    if (this->n_subformulae() == 0 || other.n_subformulae() == 0) {
        return 1.0;
    }
    const double shared_subformulae =
        static_cast<double>(this->shared_subformulae(other));
    return 0.5 *
           (shared_subformulae / static_cast<double>(this->n_subformulae()) +
            shared_subformulae / static_cast<double>(other.n_subformulae()));
}

size_t Formula::n_subformulae() const { return m_impl->m_nodes.size(); }

size_t Formula::shared_subformulae(const Formula& other) const {
    auto collect_subformula_signatures = [](const std::vector<Node>& nodes) {
        std::vector<std::string> signatures(nodes.size());
        std::unordered_map<std::string, std::size_t> counts;
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const Node& node = nodes[index];
            switch (node.m_type) {
                case NodeType::Variable:
                    signatures[index] = "V(" + node.m_variable + ")";
                    break;
                case NodeType::Not:
                    signatures[index] = "N(" + signatures[node.m_left] + ")";
                    break;
                case NodeType::And:
                    signatures[index] = "A(" + signatures[node.m_left] + "," +
                                        signatures[node.m_right] + ")";
                    break;
                case NodeType::Or:
                    signatures[index] = "O(" + signatures[node.m_left] + "," +
                                        signatures[node.m_right] + ")";
                    break;
                case NodeType::Implies:
                    signatures[index] = "I(" + signatures[node.m_left] + "," +
                                        signatures[node.m_right] + ")";
                    break;
                case NodeType::Iff:
                    signatures[index] = "E(" + signatures[node.m_left] + "," +
                                        signatures[node.m_right] + ")";
                    break;
            }
            ++counts[signatures[index]];
        }

        return counts;
    };
    const auto counts = collect_subformula_signatures(m_impl->m_nodes);
    const auto other_counts =
        collect_subformula_signatures(other.m_impl->m_nodes);
    std::size_t shared_subformulae = 0;
    for (const auto& [signature, count] : counts) {
        const auto other_it = other_counts.find(signature);
        if (other_it == other_counts.end()) {
            continue;
        }
        shared_subformulae += std::min(count, other_it->second);
    }
    return shared_subformulae;
}
