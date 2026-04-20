#include <memory>
#include <string>
#include <utility>

#include "internal.hpp"

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

bool Formula::Impl::operator<(const Impl& rhs) const {
    return m_nodes < rhs.m_nodes;
}

bool operator<(const Formula& lhs, const Formula& rhs) {
    return *lhs.m_impl < *rhs.m_impl;
}
