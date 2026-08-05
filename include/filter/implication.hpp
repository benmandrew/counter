#pragma once

/// @file implication.hpp
/// @brief Population filters based on logical implication: weakening filter,
///        deduplication, and maximal-element (implication partial order)
///        filter.

#include <atomic>
#include <cstddef>

#include "genetic/generation.hpp"
#include "runner/black.hpp"

/// Counters for the most recent make_implication_filter pairwise sweep, reset
/// at the start of each invocation of the returned FilterFunction. n_timeouts
/// is the exception: spec_implies increments it wherever it is called, so the
/// weakening filter (which resets nothing) also contributes to it.
struct ImplicationFilterStats {
    /// Unordered pairs for which the dominance check actually ran.
    inline static std::atomic<std::size_t> n_comparisons{0};
    /// Unordered pairs skipped because one endpoint was already known
    /// subsumed by an earlier comparison.
    inline static std::atomic<std::size_t> n_skipped{0};
    /// Specs that were exact duplicates of an earlier spec in the
    /// population, and so were excluded from the pairwise sweep entirely
    /// (their result is copied from their representative instead).
    inline static std::atomic<std::size_t> n_duplicates{0};
    /// black calls that timed out (inconclusive) during this sweep.
    inline static std::atomic<std::size_t> n_timeouts{0};
};

/// Returns a FilterFunction that keeps only specifications that are logical
/// weakenings of @p original — i.e. those that @p original logically implies.
///
/// A candidate is retained when original => candidate: every behaviour allowed
/// by the original is also allowed by the candidate. The same sufficient
/// assume-guarantee decomposition used by make_implication_filter is applied.
///
/// @param original  The reference specification; captured by value
/// @param checker   Satisfiability checker; captured by reference, must
///                  outlive the returned FilterFunction
FilterFunction make_weakening_filter(Specification original,
                                     SatisfiabilityChecker& checker);

/// Returns a FilterFunction that removes syntactically identical (structurally
/// equal) duplicate specifications, keeping the first occurrence of each
/// distinct spec in the input order.
FilterFunction make_dedup_filter();

/// Returns a FilterFunction that keeps only the maximal specifications of the
/// population under the implication partial order.
///
/// Spec A strictly dominates spec B when A logically implies B (A & !B is
/// unsatisfiable) but B does not imply A. Mutually equivalent specifications
/// (A implies B and B implies A) are both retained.
///
/// The unordered pairwise checks run in parallel, but far fewer than n*(n-1)/2
/// of them actually run: structurally equal specs collapse to one
/// representative before the sweep, and a pair is skipped outright once either
/// endpoint is already known subsumed.
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
