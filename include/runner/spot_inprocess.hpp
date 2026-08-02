#pragma once

/// @file spot_inprocess.hpp
/// @brief The calls this project makes into the linked libspot, in place of
///        spawning the equivalent Spot command-line tool.
///
/// Everything that talks to libspot lives here, for one reason: every such call
/// has to be serialised behind the *same* process-wide lock. Splitting them
/// across headers would make it easy to add a second entry point with a second
/// lock, which is no protection at all.
///
/// That lock is not caution, it is a measured requirement. The contended state
/// is not in any Spot object the caller holds -- it is process-global
/// underneath: SPOT's Bison and Flex parser globals, and the `robin_hood` table
/// that interns formula nodes. Giving each thread its own simplifier or
/// translator therefore does not help, and crashes even when every call is
/// locked, because construction alone reaches that state. See PROFILING.md.
///
/// Serialising is affordable because what is removed is so much larger than
/// what is serialised: about 8 ms of process startup per call, against 0.02 ms
/// of simplification or 0.16 ms of translation on real workloads.

#include <chrono>
#include <optional>
#include <string>

/// Simplifies @p formula, the in-process equivalent of `ltlfilt --simplify`.
/// Returns std::nullopt when the formula does not parse -- the caller should
/// then leave it alone, as the exec path does.
///
/// The result is byte-identical to the tool, which needs simplification level 3
/// rather than the library default; the two disagree on about 5% of formulae,
/// so the level is not a detail that can be left to the default.
std::optional<std::string> spot_simplify(const std::string& formula);

/// The outcome of a simplification that was only willing to wait so long.
struct SpotSimplification {
    /// The simplified formula; empty when the formula did not parse, or when
    /// the attempt was abandoned.
    std::optional<std::string> m_formula;
    /// True when the lock could not be taken within the budget and nothing was
    /// attempted, so the caller should spawn `ltlfilt` instead.
    bool m_lock_busy = false;
};

/// As spot_simplify, but gives up if the process-wide libspot lock cannot be
/// taken within @p budget.
///
/// This exists because in-process simplification is not always the cheaper
/// option, and which one wins depends on the workload rather than on anything
/// knowable in advance. Simplifying `fsm`'s formulae takes about 0.15 ms, so
/// serialising is free and skipping the spawn is a clear gain. Simplifying
/// `lift`'s takes about 24 ms, and serialising those puts more work on one
/// thread than the whole run has to spare, while separate `ltlfilt` processes
/// would have run them in parallel.
///
/// The budget resolves that without having to predict the cost: wait roughly as
/// long as spawning would have taken, and if the lock has not come free by
/// then, spawning is the better deal by definition. Both paths produce
/// identical output, so which one a call takes does not affect the result.
SpotSimplification spot_try_simplify(const std::string& formula,
                                     std::chrono::milliseconds budget);

/// The outcome of one translation. `m_hoa` holds the automaton, and is empty
/// when the formula did not parse or when the tautology bug below fired.
struct SpotTranslation {
    std::optional<std::string> m_hoa;
    /// True when the formula is a tautology and SPOT 2.15.1 refused to print
    /// the universal automaton it had just built, because that automaton is
    /// complete while its `prop_complete()` flag was left unset. This is the
    /// same defect that makes the `ltl2tgba` binary exit 2 on a tautology, and
    /// it is in the library rather than the command-line tool, so it has to be
    /// handled on both paths. The caller substitutes the universal automaton,
    /// exactly as it does for the exec's exit 2.
    bool m_tautology_print_bug = false;
    /// True when the libspot lock could not be taken within the budget and
    /// nothing was attempted, so the caller should spawn `ltl2tgba` instead.
    /// The same trade as for simplification: past the cost of a spawn, spawning
    /// is cheaper than queueing, and it keeps a workload with expensive
    /// translations from serialising onto one thread.
    bool m_lock_busy = false;
};

/// Translates @p formula into an automaton in HOA form, the in-process
/// equivalent of `ltl2tgba -D -S -H -f <formula>`.
///
/// The output matches the tool exactly apart from the `name:` line, which the
/// tool fills with its own simplified rendering of the formula and which
/// nothing in this project reads.
///
/// Each call translates against a fresh `bdd_dict`. Sharing one across calls is
/// measurably no faster and renumbers atomic propositions, since a dictionary
/// carries over the propositions earlier formulae registered; a fresh one
/// reproduces the tool's numbering.
///
/// Unlike the exec path this cannot be given a deadline: there is no process to
/// kill, and C++ has no way to cancel a running call. A caller that needs one
/// has to keep spawning `ltl2tgba`.
SpotTranslation spot_translate_for_counting(const std::string& formula,
                                            std::chrono::milliseconds budget);
