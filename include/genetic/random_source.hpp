#pragma once

/// @file random_source.hpp
/// @brief RandomSource abstraction wrapping a generator function, enabling
///        deterministic seeding and easy injection in tests.

#include <cassert>
#include <cstddef>
#include <functional>
#include <optional>
#include <random>
#include <utility>

/// @brief A source of randomness for genetic algorithm operations, abstracting
/// away the underlying random generator and allowing for easy injection of
/// different random sources (e.g., for testing or reproducibility).
///
/// Two calls that both draw from a RandomSource must never be arguments of the
/// same call: argument evaluation order is unspecified, and gcc and clang pick
/// opposite orders, so the draws swap between compilers and a seed stops
/// reproducing. Sequence each draw into its own local instead.
class RandomSource {
   public:
    RandomSource() = default;

    explicit RandomSource(std::function<std::size_t(std::size_t)> generator)
        : m_fn(std::move(generator)) {}

    RandomSource(std::function<std::size_t(std::size_t)> generator,
                 std::size_t seed)
        : m_fn(std::move(generator)), m_seed(seed) {}

    // Move-only. The generator owns its engine by value (see
    // make_random_source_from_seed), so a copy would fork the stream and
    // advance independently of the original -- silently breaking seed
    // reproducibility rather than failing. Pass by const reference instead;
    // next_index is const, so a const reference is all any caller needs.
    RandomSource(const RandomSource&) = delete;
    RandomSource& operator=(const RandomSource&) = delete;
    RandomSource(RandomSource&&) = default;
    RandomSource& operator=(RandomSource&&) = default;

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
        constexpr std::size_t k_resolution = 1000000;
        return static_cast<double>(next_index(k_resolution)) /
               static_cast<double>(k_resolution);
    }

    explicit operator bool() const { return static_cast<bool>(m_fn); }

    /// The seed used to initialise this source, if it was created via
    /// make_random_source_from_seed; std::nullopt for unseeded sources.
    [[nodiscard]] std::optional<std::size_t> seed() const { return m_seed; }

   private:
    std::function<std::size_t(std::size_t)> m_fn;
    std::optional<std::size_t> m_seed;
};

/// @brief Creates a RandomSource from a given seed, using `std::mt19937` as the
/// underlying generator.
/// @return RandomSource initialized with the given seed.
inline RandomSource make_random_source_from_seed(std::size_t seed) {
    std::mt19937 rng(seed);
    auto generator = [rng](std::size_t upper_bound) mutable {
        std::uniform_int_distribution<std::size_t> dist(0, upper_bound - 1);
        return dist(rng);
    };
    return {generator, seed};
}
