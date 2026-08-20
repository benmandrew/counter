#pragma once

#include <cstddef>
#include <cstdint>

/// Folds @p value into @p seed, boost-style.
///
/// Every hasher in the codebase reimplemented this locally, and three of the
/// four copies carried the 32-bit constant `0x9e3779b9` on a 64-bit
/// `std::size_t`, so the top half of the seed was barely mixed. The arithmetic
/// is done in `std::uint64_t` rather than `std::size_t` so the constant is the
/// same width as the accumulator on every target.
constexpr std::size_t hash_combine(std::size_t seed,
                                   std::size_t value) noexcept {
    const auto wide = static_cast<std::uint64_t>(seed);
    return static_cast<std::size_t>(wide ^ (static_cast<std::uint64_t>(value) +
                                            0x9e3779b97f4a7c15ULL +
                                            (wide << 6U) + (wide >> 2U)));
}
