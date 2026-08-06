#pragma once

/// @file spot.hpp
/// @brief Wrappers for SPOT tools: ltl2tgba (automaton construction) and
///        ltlsynt (realizability checking), with memoising checker classes.

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "requirement.hpp"

/// Returns the directory containing SPOT tool binaries, set at build time via
/// the SPOT_BIN_DIR preprocessor definition.
std::string spot_bin_dir();

/// Returns the full filesystem path to the ltlsynt binary.
std::string ltlsynt_path();

/// Returns the full filesystem path to the ltl2tgba binary.
std::string ltl2tgba_path();

/// Runs ltl2tgba with -D (deterministic), -S (state-based acceptance), and
/// -H (HOA output) on the given LTL formula and returns the raw HOA text.
/// Asserts that the binary is accessible; the result is memoised by formula.
///
/// None of the three non-clean exits is an assertion. A non-zero exit throws,
/// and so does a timeout (see set_ltl2tgba_timeout), which drops the
/// individual. SPOT's exit 2 on a tautology is neither: the universal
/// automaton is substituted for it, because a formula accepting every trace
/// is a correct answer rather than a scoring failure (PR #29 — still unfixed
/// upstream).
std::string run_ltl2tgba_for_counting(const std::string& formula);

/// Per-call wall-clock budget for the ltl2tgba counting exec (process-global,
/// like RealizabilityChecker's ltlsynt timeout). A call exceeding it is killed
/// and raised as an error, so the offending individual is dropped rather than
/// stalling the run on an unbounded deterministic-automaton blowup. Zero (the
/// default) disables the timeout. Set once at startup from
/// Config::ltl2tgba_timeout.
void set_ltl2tgba_timeout(std::chrono::milliseconds timeout);

struct Ltl2tgbaStats {
    inline static std::size_t n_cache_hits = 0;
    inline static std::size_t n_cache_misses = 0;
    inline static double total_time_s = 0.0;
    /// Child-process CPU time (user+sys), from wait4(); unlike total_time_s
    /// (wall) it excludes time the parent spends blocked waiting on the child.
    inline static double total_cpu_s = 0.0;
    /// ltl2tgba exit-2-on-tautology results substituted with the universal
    /// automaton (see run_ltl2tgba_for_counting) rather than raised as errors.
    inline static std::size_t n_tautology_substitutions = 0;
    /// Counting calls abandoned at the per-call timeout (raised as errors, so
    /// the individual is dropped). Counts execs, not calls: the abandonment is
    /// memoised, and a repeat of the same formula re-raises it as a cache hit
    /// without paying the budget again.
    inline static std::size_t n_timeouts = 0;

    /// Folds one exec's wall and child-CPU time into the totals. The caller
    /// must hold the stats/cache mutex, matching the other accumulators here.
    static void record_time(double wall_s, double cpu_s) {
        total_time_s += wall_s;
        total_cpu_s += cpu_s;
    }
};

class RealizabilityChecker {
   public:
    inline static std::size_t n_cache_misses = 0;
    inline static std::size_t n_cache_hits = 0;
    inline static double total_time_s = 0.0;
    inline static double total_cpu_s = 0.0;
    /// ltlsynt calls abandoned at the per-call timeout, reported as nullopt.
    /// Counts execs, not calls: the undecided outcome is memoised like any
    /// other, so a formula that times out costs one exec however often the
    /// search revisits it.
    inline static std::size_t n_timeouts = 0;

    /// Checks whether the specification is realizable using ltlsynt. Returns
    /// true (realizable), false (unrealizable), or nullopt (timed out, so
    /// undecided). Results are memoised by the full specification formula, so
    /// repeated calls with identical inputs incur no additional tool
    /// invocations.
    std::optional<bool> check_realizability(const Specification& specification);

    /// Realizability of a raw LTL formula with the given input/output atom
    /// partition (mode-agnostic core shared by the FRETISH and TLSF front
    /// ends). Memoised by the formula verbatim together with the atom
    /// partition; the formula is deliberately *not* normalised first, for the
    /// reason given where check_realizability_ltl is defined in spot.cpp.
    ///
    /// nullopt means undecided rather than unrealizable, and the distinction
    /// is the caller's to resolve: "unrealizable" is the safe reading where a
    /// true answer admits a repair, and the unsafe one for the well-separation
    /// filter, which keeps a candidate exactly when the query comes back
    /// unrealizable. Spell the choice with value_or at each call site.
    std::optional<bool> check_realizability_ltl(
        const std::string& ltl_formula, const std::vector<std::string>& inputs,
        const std::vector<std::string>& outputs);

    /// Caps the number of ltlsynt processes running concurrently across the
    /// whole program. The gate is process-global (shared by every
    /// RealizabilityChecker, including test instances) because the memory
    /// pressure it guards against is process-global: ltlsynt is multi-GB
    /// resident per call on hard specs, so an uncapped scoring pool can OOM the
    /// machine. 0 (the default) means unlimited. Set once at startup from
    /// Config::max_concurrent_realizability.
    static void set_max_concurrency(std::size_t limit);

    /// Per-call wall-clock budget for the ltlsynt exec (process-global, like
    /// the concurrency gate). A call exceeding it is killed and reported as
    /// undecided — ltlsynt has no internal timeout, and the genetic search
    /// occasionally generates synthesis queries that run for minutes. Zero (the
    /// default) disables the timeout. Set once at startup from
    /// Config::ltlsynt_timeout.
    static void set_timeout(std::chrono::milliseconds timeout);

   private:
    mutable std::mutex m_cache_mutex;
    /// Undecided (nullopt) is memoised alongside the decided verdicts, as
    /// black's satisfiability cache does: ltlsynt is deterministic and its
    /// call durations are sharply bimodal, so a formula that blew the budget
    /// once is in the minutes-long tail rather than near the boundary, and
    /// re-asking it re-pays the budget for the same non-answer. Caching it is
    /// safe in a way caching a fabricated `false` was not, because nullopt is
    /// not a verdict: each caller still resolves it in its own direction.
    std::unordered_map<std::string, std::optional<bool>> m_cache;
};

/// Returns the process-lifetime RealizabilityChecker instance. All callers
/// that do not need test isolation should use this instead of constructing
/// their own, so they share the memoisation cache.
RealizabilityChecker& global_real_checker();
