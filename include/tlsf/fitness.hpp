#pragma once

/// @file fitness.hpp
/// @brief TLSF analogues of the FRETISH fitness components (syntactic,
///        semantic, Halstead, status) and the factory assembling them into a
///        weighted aggregate over tlsf::Specification.

#include "config.hpp"
#include "fitness/function.hpp"
#include "tlsf/specification.hpp"

/// Positional per-section syntactic similarity of @p spec against @p original.
/// For each of the six sections, formula i of @p spec is paired with formula i
/// of @p original over `min(size)`; a size difference contributes similarity 0
/// for each missing pair. The result is the average of Formula::syntactic
/// similarity over all such pairs across all sections, in [0, 1]. When both
/// specifications hold no formulae the result is 1.0.
double tlsf_syntactic_similarity(const tlsf::Specification& spec,
                                 const tlsf::Specification& original,
                                 const Config& cfg);

/// Positional per-section semantic similarity of @p spec against @p original.
/// Identical formula pairs are excluded; each differing pair contributes the
/// harmonic mean of its two bounded-model-counting containment ratios at bound
/// `cfg.default_model_counting_bound`. The result averages over the differing
/// pairs, in [0, 1]. When no pair differs the result is 1.0.
double tlsf_semantic_similarity(const tlsf::Specification& spec,
                                const tlsf::Specification& original,
                                const Config& cfg);

/// Halstead size penalty of @p spec relative to @p original, in [0, 1]. Token
/// counts are summed over every section formula of each specification; the
/// score is `min(1, volume(original) / volume(spec))`, so a candidate no larger
/// than the original scores 1.0 and larger candidates score lower.
double tlsf_halstead_fitness(const tlsf::Specification& spec,
                             const tlsf::Specification& original,
                             const Config& cfg);

/// Tiered realizability status of @p spec, in [0, 1]:
///   0.0 — some section formula is individually unsatisfiable
///   0.1 — the guarantee-side conjunction is unsatisfiable
///   0.2 — the assumption-side conjunction is unsatisfiable
///   0.5 — everything satisfiable but the lowering is unrealizable
///   1.0 — the lowering is realizable
double tlsf_status(const tlsf::Specification& spec, const Config& cfg);

/// Builds the weighted aggregate of the four TLSF fitness components, gated on
/// the same `cfg.fitness_weight_*` fields the FRETISH factory uses. Components
/// with a non-positive weight are omitted.
AggregateWeightedFitnessFunctionT<tlsf::Specification>
tlsf_get_fitness_function(const tlsf::Specification& original,
                          const Config& cfg);
