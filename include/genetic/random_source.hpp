#pragma once

#include <functional>

using RandomSource = std::function<bool()>;

inline int next_2bit_selector(const RandomSource& random_source) {
    const bool high_bit = random_source();
    const bool low_bit = random_source();
    return (high_bit ? 2 : 0) + (low_bit ? 1 : 0);
}
