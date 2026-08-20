#pragma once

/// @file random_source.hpp
/// @brief RandomSource abstraction wrapping a generator function, enabling
///        deterministic seeding and easy injection in tests.

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
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

    /// Move-only. The generator owns its engine by value (see
    /// make_random_source_from_seed), so a copy would fork the stream and
    /// advance independently of the original -- silently breaking seed
    /// reproducibility rather than failing. Pass by const reference instead;
    /// next_index is const, so a const reference is all any caller needs.
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
/// A uniform draw in [0, bound), reduced from one 32-bit engine word by
/// Lemire's method: multiply into 64 bits and take the high half, rejecting
/// only the low tail that would bias the result.
///
/// Daniel Lemire, "Fast Random Integer Generation in an Interval", ACM
/// Transactions on Modelling and Computer Simulation 29(1), 2019, article 3.
/// https://doi.org/10.1145/3230636 — arXiv:1805.10941. Vendored here rather
/// than depended on: it is the eighteen lines below and nothing else.
///
/// Vendored rather than taken from std::uniform_int_distribution because that
/// class is not specified to produce any particular value. libstdc++ and
/// libc++ disagree on every bound, so a seed reproduced a run only within one
/// standard library -- which made a macOS run unable to reproduce a Linux
/// campaign at all. std::mt19937 above needs no such treatment: the standard
/// pins it exactly, down to a required test vector.
///
/// The choice of Lemire specifically is what makes this free on Linux. It is
/// what libstdc++ already does, so adopting it leaves every archived seed, and
/// the determinism goldens, meaning exactly what they meant before; only macOS
/// moves, and it moves onto Linux's stream. golden_bounded_uniform in the
/// determinism suite pins that, so a future libstdc++ change cannot drift it
/// back without failing a test.
inline std::uint32_t bounded_uniform(std::mt19937& rng, std::uint32_t bound) {
    // A bound of 1 has one legal answer, but it still draws: libstdc++ spends
    // an engine word on it, and returning early without one would slide every
    // later draw up by a place. The general path below already handles it --
    // the high half of draw*1 is 0 and the rejection test cannot fire -- so it
    // costs a word and returns 0, which is what makes this a drop-in.
    assert(bound != 0);
    std::uint32_t draw = rng();
    std::uint64_t wide = static_cast<std::uint64_t>(draw) * bound;
    auto low = static_cast<std::uint32_t>(wide);
    if (low < bound) {
        // 2^32 % bound, the size of the biased tail, and computed only on
        // this rare branch -- which is what "nearly divisionless" refers to.
        // Spelled without unary minus on an unsigned type: bound is itself a
        // multiple of bound, so subtracting it leaves the residue unchanged
        // and the arithmetic stays inside the type.
        const std::uint32_t threshold =
            ((std::numeric_limits<std::uint32_t>::max() - bound) + 1) % bound;
        while (low < threshold) {
            draw = rng();
            wide = static_cast<std::uint64_t>(draw) * bound;
            low = static_cast<std::uint32_t>(wide);
        }
    }
    return static_cast<std::uint32_t>(wide >> 32);
}

inline RandomSource make_random_source_from_seed(std::size_t seed) {
    std::mt19937 rng(seed);
    auto generator = [rng](std::size_t upper_bound) mutable {
        // Every bound the search draws against is a population size, a
        // container index or the fixed 1e6 of next_real, so none approaches
        // 2^32; the reduction is 32-bit for that reason.
        assert(upper_bound <= std::numeric_limits<std::uint32_t>::max());
        return static_cast<std::size_t>(
            bounded_uniform(rng, static_cast<std::uint32_t>(upper_bound)));
    };
    return {generator, seed};
}
