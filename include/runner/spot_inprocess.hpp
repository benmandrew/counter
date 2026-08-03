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
#include <cstddef>
#include <optional>
#include <string>

/// Simplifies @p formula, the in-process equivalent of `ltlfilt --simplify`.
/// Returns std::nullopt when the formula does not parse -- the caller should
/// then leave it alone, as the exec path does.
///
/// Simplification runs at level 3, which is what `--simplify` selects; the
/// library default is weaker and disagrees on about 5% of formulae, so the
/// level is not a detail that can be left alone.
///
/// The result is *equivalent* to the tool's, not byte-identical to it, and the
/// difference is worth understanding before relying on either. SPOT prints the
/// operands of commutative operators in formula-node id order, and ids are
/// assigned when a node is first interned -- into a table that is global to the
/// process and lives as long as it does. `ltlfilt` looked like a pure function
/// of its input only because every call got a fresh process, and so a fresh
/// table. In process, the same formula can print as `Ga | Fb` or `Fb | Ga`
/// depending on what was simplified before it.
///
/// Measured: a cold process reproduces the tool byte for byte; after a handful
/// of unrelated calls, about a fifth of a random corpus prints differently. The
/// two are always logically equivalent, and always differ only by that
/// ordering -- both pinned over a corpus in test/runner/differential_tests.cpp,
/// and checked end-to-end across engines, thread counts and repeated runs by
/// scripts/check_engine_parity.py, which has yet to find a difference that
/// reaches a repair.
std::optional<std::string> spot_simplify(const std::string& formula);

/// The outcome of a simplification that was only willing to wait so long.
struct SpotSimplification {
    /// The simplified formula; empty when the formula did not parse, or when
    /// the attempt was abandoned.
    std::optional<std::string> m_formula;
    /// True when the lock could not be taken within the budget and nothing was
    /// attempted, so the caller should spawn `ltlfilt` instead.
    bool m_lock_busy = false;
    /// True when the deadline expired before the simplification finished. The
    /// call was abandoned and nothing will ever deliver its answer, so the
    /// caller should use the formula unsimplified -- which is what it already
    /// does when there is no `ltlfilt` to fall back to. Unlike a translation,
    /// a simplification that does not arrive costs only the size of a formula,
    /// never its meaning.
    bool m_timed_out = false;
};

/// As spot_simplify, but gives up if the process-wide libspot lock cannot be
/// taken within @p budget, or if the simplification itself outruns
/// @p deadline (zero, the default meaning of `simplify_timeout_ms`, being
/// unbounded).
///
/// The budget exists because in-process simplification is not always the
/// cheaper option, and which one wins depends on the workload rather than on
/// anything knowable in advance. Simplifying `fsm`'s formulae takes about
/// 0.15 ms, so serialising is free and skipping the spawn is a clear gain.
/// Simplifying `lift`'s takes about 24 ms, and serialising those puts more work
/// on one thread than the whole run has to spare, while separate `ltlfilt`
/// processes would have run them in parallel.
///
/// The budget resolves that without having to predict the cost: wait roughly as
/// long as spawning would have taken, and if the lock has not come free by
/// then, spawning is the better deal by definition. Both paths produce
/// equivalent output, so which one a call takes does not affect the result.
///
/// The deadline covers the other half: how long the work may run once it has
/// the lock. Simplification has no internal bound and `--simplify` blows up
/// super-exponentially on deeply nested-X conjunctions -- about a second at 12
/// ticks, twenty at 15 -- which is the shape this search builds. Enforcing it
/// costs a thread and abandons the call; see spot_translate_for_counting for
/// what abandoning means and why it is safe.
SpotSimplification spot_try_simplify(const std::string& formula,
                                     std::chrono::milliseconds budget,
                                     std::chrono::milliseconds deadline);

/// The outcome of one translation. `m_hoa` holds the automaton, and is empty
/// when the formula did not parse, when the tautology bug below fired, or when
/// the deadline expired.
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
    /// True when the deadline expired before the translation finished. The
    /// automaton is not merely late but gone: the call was abandoned, and
    /// nothing will ever deliver it. Treat it exactly as the exec path treats
    /// a killed `ltl2tgba` -- drop the individual.
    bool m_timed_out = false;
};

/// Translates @p formula into an automaton in HOA form, the in-process
/// equivalent of `ltl2tgba -D -S -H -f <formula>`.
///
/// The output matches the tool apart from the `name:` line, which the tool
/// fills with its own simplified rendering of the formula and which nothing in
/// this project reads -- and apart from the ordering of `AP:`, for the reason
/// spot_simplify describes above. A warm intern table lists the atomic
/// propositions in a different order and renumbers every edge label to match,
/// which is a renaming rather than a different automaton: the two accept the
/// same language, and the trace counts taken from them are the same.
///
/// Each call translates against a fresh `bdd_dict`. Sharing one across calls is
/// measurably no faster and renumbers atomic propositions, since a dictionary
/// carries over the propositions earlier formulae registered; a fresh one
/// reproduces the tool's numbering.
///
/// @p deadline bounds the translation itself, and zero (the default meaning of
/// `ltl2tgba_timeout_ms`) means unbounded. Note what a deadline can and cannot
/// mean here. The exec path enforces one by killing the process doing the work;
/// in process there is nothing to kill, and C++ offers no way to cancel a call
/// already running inside libspot. So the deadline is honoured by *abandoning*
/// the call rather than stopping it: the work is handed to a worker thread that
/// takes the libspot lock with it, and if the deadline passes the caller
/// returns `m_timed_out` while that worker runs on.
///
/// The consequence is deliberate and is the reason this is safe to do at all.
/// An abandoned worker keeps the lock, so every later call -- translation and
/// simplification alike -- finds it busy within the budget and spawns the tool
/// instead. One pathological formula therefore costs the process its in-process
/// fast path until that formula finishes, and nothing more; it cannot stall the
/// run, because no caller ever waits on it again. When the worker does finish
/// it releases the lock and the fast path resumes on its own.
///
/// What is genuinely given up is a thread and whatever memory the translation
/// holds, for as long as it runs. That is strictly worse than killing a child
/// process, and it is the price of not paying ~10 ms of startup on every call.
/// spot_abandoned_workers() reports how many are outstanding.
SpotTranslation spot_translate_for_counting(const std::string& formula,
                                            std::chrono::milliseconds budget,
                                            std::chrono::milliseconds deadline);

/// How many calls -- translations or simplifications -- have been abandoned
/// and are still running.
///
/// Worth checking before the process exits: static destruction while one of
/// these is inside libspot tears down state it is still using. Anything above
/// zero also says the in-process fast path is currently disabled, since an
/// abandoned worker holds the lock.
std::size_t spot_abandoned_workers();
