#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "hash_combine.hpp"
#include "internal.hpp"

Formula::Formula() : m_impl(std::make_unique<Impl>("true")) {}

Formula Formula::true_formula = Formula("true");
Formula Formula::false_formula = Formula("false");

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

bool Formula::Impl::operator<(const Impl& rhs) const {
    return m_nodes < rhs.m_nodes;
}

bool operator<(const Formula& lhs, const Formula& rhs) {
    return *lhs.m_impl < *rhs.m_impl;
}

bool Formula::is_propositional() const {
    for (const prop_formula_internal::Node& node : m_impl->m_nodes) {
        switch (node.m_type) {
            case prop_formula_internal::NodeType::Variable:
            case prop_formula_internal::NodeType::Not:
            case prop_formula_internal::NodeType::And:
            case prop_formula_internal::NodeType::Or:
            case prop_formula_internal::NodeType::Implies:
            case prop_formula_internal::NodeType::Iff:
                break;
            case prop_formula_internal::NodeType::Next:
            case prop_formula_internal::NodeType::Eventually:
            case prop_formula_internal::NodeType::Globally:
            case prop_formula_internal::NodeType::Until:
            case prop_formula_internal::NodeType::Release:
            case prop_formula_internal::NodeType::WeakUntil:
                return false;
        }
    }
    return true;
}

// Hashes the raw node arena, as operator< above compares it: two logically
// identical formulae with different arena layouts are unequal and hash
// differently, which is what forces construction and extraction to preserve
// the parser's layout (see transform.cpp).
std::size_t Formula::hash() const noexcept {
    using prop_formula_internal::Node;
    std::size_t seed = 0;
    for (const Node& node : m_impl->m_nodes) {
        seed = hash_combine(seed, std::hash<std::uint8_t>{}(
                                      static_cast<std::uint8_t>(node.m_type)));
        seed = hash_combine(seed, std::hash<std::string>{}(node.m_variable));
        seed = hash_combine(seed, std::hash<std::size_t>{}(node.m_left));
        seed = hash_combine(seed, std::hash<std::size_t>{}(node.m_right));
    }
    return seed;
}
