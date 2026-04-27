#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>

/// Wraps a callable that returns pseudo-random values in [0, upper_bound).
class RandomSource {
   public:
    RandomSource() = default;

    explicit RandomSource(std::function<std::size_t(std::size_t)> fn)
        : m_fn(std::move(fn)) {}

    /// Returns a pseudo-random index in [0, upper_bound).
    std::size_t next_index(std::size_t upper_bound) const {
        if (!m_fn) {
            throw std::invalid_argument("random source must be callable.");
        }
        if (upper_bound == 0) {
            throw std::invalid_argument("upper_bound must be positive.");
        }
        return m_fn(upper_bound) % upper_bound;
    }

    /// Returns a pseudo-random boolean.
    bool next_bool() const { return next_index(2) == 1; }

    explicit operator bool() const { return static_cast<bool>(m_fn); }

   private:
    std::function<std::size_t(std::size_t)> m_fn;
};
