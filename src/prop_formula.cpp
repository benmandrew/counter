#include "prop_formula.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <memory>
#include <optional>
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
    throw std::logic_error("Unknown formula node type.");
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
        throw std::logic_error("Unknown formula node type.");
    };

    return to_string_recursive(index);
}

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

Formula::Formula() : m_impl(std::make_unique<Impl>("true")) {}

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

Formula Formula::make_atom(const std::string& atom) { return Formula(atom); }

Formula Formula::make_unary(Kind kind, const Formula& child) {
    if (kind != Kind::Not) {
        throw std::invalid_argument(
            "make_unary supports only Kind::Not for propositional formulae.");
    }
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
            throw std::invalid_argument(
                "make_binary requires a binary operator kind.");
    }
    throw std::logic_error("Unknown formula kind.");
}

Formula::Kind Formula::kind() const {
    if (m_impl->m_nodes.empty()) {
        throw std::logic_error("Formula has no root node.");
    }
    return node_type_to_kind(m_impl->m_nodes.back().m_type);
}

std::optional<std::string> Formula::atom_name() const {
    const Node& root = m_impl->m_nodes.back();
    if (root.m_type != NodeType::Variable) {
        return std::nullopt;
    }
    return root.m_variable;
}

std::optional<Formula> Formula::unary_child() const {
    const Node& root = m_impl->m_nodes.back();
    if (root.m_type != NodeType::Not) {
        return std::nullopt;
    }
    return Formula(node_to_string(m_impl->m_nodes, root.m_left));
}

std::optional<std::pair<Formula, Formula>> Formula::binary_children() const {
    const Node& root = m_impl->m_nodes.back();
    switch (root.m_type) {
        case NodeType::And:
        case NodeType::Or:
        case NodeType::Implies:
        case NodeType::Iff:
            return std::make_pair(
                Formula(node_to_string(m_impl->m_nodes, root.m_left)),
                Formula(node_to_string(m_impl->m_nodes, root.m_right)));
        case NodeType::Variable:
        case NodeType::Not:
            return std::nullopt;
    }
    throw std::logic_error("Unknown formula node type.");
}

Formula Formula::rewrite_post_order(
    const RewriteCallback& rewrite_callback) const {
    if (!rewrite_callback) {
        return *this;
    }

    std::function<Formula(std::size_t)> rewrite_subtree =
        [this, &rewrite_subtree,
         &rewrite_callback](std::size_t index) -> Formula {
        const Node& node = m_impl->m_nodes[index];
        Formula rewritten_subtree;
        switch (node.m_type) {
            case NodeType::Variable:
                rewritten_subtree = Formula::make_atom(node.m_variable);
                break;
            case NodeType::Not: {
                const Formula child = rewrite_subtree(node.m_left);
                rewritten_subtree = Formula::make_unary(Kind::Not, child);
                break;
            }
            case NodeType::And: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree =
                    Formula::make_binary(Kind::And, left, right);
                break;
            }
            case NodeType::Or: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree = Formula::make_binary(Kind::Or, left, right);
                break;
            }
            case NodeType::Implies: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree =
                    Formula::make_binary(Kind::Implies, left, right);
                break;
            }
            case NodeType::Iff: {
                const Formula left = rewrite_subtree(node.m_left);
                const Formula right = rewrite_subtree(node.m_right);
                rewritten_subtree = Formula::make_binary(Kind::Iff, left, right);
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

std::string Formula::to_string() const {
    if (m_impl->m_nodes.empty()) {
        return "";
    }

    return node_to_string(m_impl->m_nodes, m_impl->m_nodes.size() - 1);
}
