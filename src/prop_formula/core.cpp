#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

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

std::size_t Formula::hash() const noexcept {
    using prop_formula_internal::Node;
    auto combine = [](std::size_t seed, std::size_t val) noexcept {
        return seed ^ (val + 0x9e3779b9U + (seed << 6) + (seed >> 2));
    };
    std::size_t seed = 0;
    for (const Node& node : m_impl->m_nodes) {
        seed = combine(seed, std::hash<std::uint8_t>{}(
                                 static_cast<std::uint8_t>(node.m_type)));
        seed = combine(seed, std::hash<std::string>{}(node.m_variable));
        seed = combine(seed, std::hash<std::size_t>{}(node.m_left));
        seed = combine(seed, std::hash<std::size_t>{}(node.m_right));
    }
    return seed;
}
