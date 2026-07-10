#pragma once

/// @file operators.hpp
/// @brief The GeneticOperators<Spec> bundle of crossover, mutation, and
///        simplification callables injected into the generic evolution loop.

#include <functional>

#include "config.hpp"
#include "genetic/random_source.hpp"

/// Bundles the per-element genetic operators the evolution loop applies to
/// offspring. @c simplify may be empty, in which case it is treated as the
/// identity.
template <typename Spec>
struct GeneticOperators {
    std::function<Spec(const Spec&, const Spec&, const RandomSource&)>
        crossover;
    std::function<Spec(const Spec&, const RandomSource&, const Config&)> mutate;
    std::function<Spec(Spec)> simplify;
};
