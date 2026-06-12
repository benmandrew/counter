#pragma once

#include <cassert>
#include <cstddef>
#include <functional>
#include <random>
#include <utility>

/// @brief A source of randomness for genetic algorithm operations, abstracting
/// away the underlying random generator and allowing for easy injection of
/// different random sources (e.g., for testing or reproducibility).
class RandomSource {
   public:
    RandomSource() = default;

    explicit RandomSource(std::function<std::size_t(std::size_t)> generator)
        : m_fn(std::move(generator)) {}

    /// Returns a pseudo-random index in [0, upper_bound).
    [[nodiscard]] std::size_t next_index(std::size_t upper_bound) const {
        assert(m_fn);
        assert(upper_bound != 0);
        return m_fn(upper_bound) % upper_bound;
    }

    /// Returns a pseudo-random boolean.
    [[nodiscard]] bool next_bool() const { return next_index(2) == 1; }

    /// Returns a pseudo-random double uniformly in [0, 1).
    [[nodiscard]] double next_real() const {
        constexpr std::size_t kResolution = 1000000;
        return static_cast<double>(next_index(kResolution)) /
               static_cast<double>(kResolution);
    }

    explicit operator bool() const { return static_cast<bool>(m_fn); }

   private:
    std::function<std::size_t(std::size_t)> m_fn;
};

/// @brief Creates a RandomSource from a given seed, using `std::mt19937` as the
/// underlying generator.
/// @param seed
/// @return RandomSource initialized with the given seed.
inline RandomSource make_random_source_from_seed(std::size_t seed) {
    std::mt19937 rng(seed);
    auto generator = [rng](std::size_t upper_bound) mutable {
        std::uniform_int_distribution<std::size_t> dist(0, upper_bound - 1);
        return dist(rng);
    };
    return RandomSource(generator);
}
