#include <cassert>
#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include "internal.hpp"

namespace {

class Parser {
   private:
    std::string m_text;
    std::size_t m_position = 0;
    std::vector<prop_formula_internal::Node> m_nodes;

   public:
    explicit Parser(std::string text) : m_text(std::move(text)) {}

    std::vector<prop_formula_internal::Node> parse() {
        m_position = 0;
        m_nodes.clear();
        const std::size_t root = parse_iff();
        skip_whitespace();
        assert(at_end());
        assert(!m_nodes.empty());
        if (root != m_nodes.size() - 1) {
            const prop_formula_internal::Node root_node = m_nodes[root];
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
            lhs = push_binary(prop_formula_internal::NodeType::Iff, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_implies() {
        std::size_t lhs = parse_or();
        if (try_consume("->")) {
            const std::size_t rhs = parse_implies();
            lhs =
                push_binary(prop_formula_internal::NodeType::Implies, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_or() {
        std::size_t lhs = parse_and();
        while (try_consume("|")) {
            const std::size_t rhs = parse_and();
            lhs = push_binary(prop_formula_internal::NodeType::Or, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_and() {
        std::size_t lhs = parse_unary();
        while (try_consume("&")) {
            const std::size_t rhs = parse_unary();
            lhs = push_binary(prop_formula_internal::NodeType::And, lhs, rhs);
        }
        return lhs;
    }

    std::size_t parse_unary() {
        if (try_consume("!") || try_consume("~")) {
            const std::size_t child = parse_unary();
            return push_unary(prop_formula_internal::NodeType::Not, child);
        }
        if (try_consume("(")) {
            const std::size_t expression = parse_iff();
            assert(try_consume(")"));
            return expression;
        }
        return parse_variable();
    }

    std::size_t parse_variable() {
        skip_whitespace();
        assert(!at_end());
        const char first = m_text[m_position];
        assert(std::isalpha(static_cast<unsigned char>(first)) || first == '_');
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

        m_nodes.push_back(prop_formula_internal::Node{
            prop_formula_internal::NodeType::Variable, name, 0, 0});
        return m_nodes.size() - 1;
    }

    std::size_t push_unary(prop_formula_internal::NodeType type,
                           std::size_t child) {
        m_nodes.push_back(prop_formula_internal::Node{type, "", child, 0});
        return m_nodes.size() - 1;
    }

    std::size_t push_binary(prop_formula_internal::NodeType type,
                            std::size_t lhs, std::size_t rhs) {
        m_nodes.push_back(prop_formula_internal::Node{type, "", lhs, rhs});
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
};

}  // namespace

namespace prop_formula_internal {

std::vector<Node> parse_formula(const std::string& formula) {
    Parser parser(formula);
    return parser.parse();
}

}  // namespace prop_formula_internal

Formula::Impl::Impl(const std::string& formula)
    : m_nodes(prop_formula_internal::parse_formula(formula)) {}
