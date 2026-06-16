#pragma once

#include <atomic>
#include <cstddef>

#include "genetic/generation.hpp"
#include "runner/black.hpp"

/// Counters for the most recent make_implication_filter pairwise sweep, reset
/// at the start of each invocation of the returned FilterFunction.
struct ImplicationFilterStats {
    /// Unordered pairs for which the dominance check actually ran.
    inline static std::atomic<std::size_t> n_comparisons{0};
    /// Unordered pairs skipped because one endpoint was already known
    /// subsumed by an earlier comparison.
    inline static std::atomic<std::size_t> n_skipped{0};
};

/// Returns a FilterFunction that keeps only the maximal specifications of the
/// population under the implication partial order.
///
/// Spec A strictly dominates spec B when A logically implies B (A & !B is
/// unsatisfiable) but B does not imply A. Mutually equivalent specifications
/// (A implies B and B implies A) are both retained.
///
/// All n*(n-1)/2 unordered pairwise implication checks run in parallel.
/// @p checker must be thread-safe (SatisfiabilityChecker satisfies this).
/// The checker is captured by reference; it must outlive the returned
/// FilterFunction.
///
/// @param checker      Satisfiability checker for pairwise implication tests
/// @param on_progress  Optional callback invoked after each batch of pairs is
///                     checked; receives (done, total) pair counts
FilterFunction make_implication_filter(
    SatisfiabilityChecker& checker,
    const GenerationProgressCallback& on_progress = nullptr);
