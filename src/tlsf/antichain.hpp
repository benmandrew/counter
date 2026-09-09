#pragma once

/// @file antichain.hpp
/// @brief The running maximal antichain over a set of specifications in
///        discovery order, and the event log it emits.
///
/// The batch alternative recomputes the maximal set of every prefix, which the
/// scoring pass does today at 20 log-spaced cuts per run. Carrying the previous
/// cut's survivors forward already removed 4.5x of that (see
/// `maximality_rows` in scripts/score_curves.py), and this removes the rest:
/// each arrival is compared against the running antichain alone, which is
/// O(n * |antichain|) rather than O(n * |prefix|), and every prefix's answer
/// falls out of one walk rather than one process per cut.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "genetic/generation.hpp"
#include "runner/black.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/specification.hpp"

namespace tlsf {

/// One specification and when the run that produced it found it.
struct Arrival {
    std::string m_name;
    double m_elapsed_s{0.0};
    Specification m_specification;
};

/// What happened to one specification at one instant.
///
/// `Admit` and `Drop` are the two verdicts on an arrival. `Remove` is what an
/// admission does to a member the new arrival dominates, and carries the
/// arrival's own timestamp, so the log is totally ordered by `m_elapsed_s` and
/// then by position.
enum class AntichainEvent : std::uint8_t { Admit, Drop, Remove };

struct AntichainRow {
    double m_elapsed_s{0.0};
    std::string m_name;
    AntichainEvent m_event{AntichainEvent::Admit};
    /// The antichain's size after this event, so a reader needs no state of
    /// its own to plot the curve.
    std::size_t m_size{0};
};

/// What the walk cost, for the progress line and for judging the prefilter.
struct AntichainStats {
    /// Ordered implication questions that reached the solver.
    std::size_t m_solver_queries{0};
    /// Ordered questions a sampled word answered instead.
    std::size_t m_refuted_directions{0};
    /// Scans that stopped at a dominating member rather than walking the whole
    /// antichain. This is the saving the batch sweep cannot have.
    std::size_t m_short_circuited{0};
    /// Comparisons among the survivors of one wave: the price of running the
    /// scans concurrently rather than one at a time.
    std::size_t m_reconciled_pairs{0};
};

/// Walks @p arrivals in the order given, maintaining the maximal antichain
/// under implication, and returns one row per event.
///
/// The order is the caller's: pass the arrivals sorted by `m_elapsed_s` and the
/// log is the anytime curve, pass any other order and the final antichain is
/// the same set, maximality being a property of the set rather than of the
/// walk.
///
/// @p wave_size arrivals are scanned concurrently against a snapshot of the
/// antichain taken before the wave, and reconciled against each other
/// afterwards. Zero means `dispatch_window()`. One makes the walk serial, which
/// is what the tests cross the concurrent walk against.
///
/// @p similarity ranks the members of an equivalence class exactly as
/// `tlsf_make_implication_filter` does; an empty one leaves the tie-break to
/// `Specification::operator<`.
///
/// @p on_row is called as each row is appended, from the serial step that
/// applies the wave, so a caller may write the log out as it is produced and
/// keep a prefix of it when the walk is killed part-way. It is never called
/// concurrently.
///
/// @p checker is captured by reference for the duration of the call and must be
/// thread-safe.
using AntichainRowCallback = std::function<void(const AntichainRow&)>;

std::vector<AntichainRow> running_antichain(
    const std::vector<Arrival>& arrivals, SatisfiabilityChecker& checker,
    const TlsfSimilarityKey& similarity = nullptr, std::size_t wave_size = 0,
    const GenerationProgressCallback& on_progress = nullptr,
    const AntichainRowCallback& on_row = nullptr,
    AntichainStats* stats = nullptr);

/// The members of the antichain at @p elapsed_s, in the order they were
/// admitted, replayed from @p rows.
///
/// A drop and a removal are both permanent -- a specification some member
/// dominates is dominated by whatever later dominates that member, implication
/// being transitive -- so membership at any instant is exactly the admissions
/// up to it less the removals up to it, with no re-derivation.
std::vector<std::string> antichain_members_at(
    const std::vector<AntichainRow>& rows, double elapsed_s);

}  // namespace tlsf
