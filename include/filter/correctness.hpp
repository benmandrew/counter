#pragma once

/// @file correctness.hpp
/// @brief The correctness properties a written repair must hold, as one list
///        per front end.

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "config.hpp"
#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"

/// One property a specification must hold to be written out as a repair.
///
/// The same row drives three consumers: the per-generation filter chain builds
/// a stage from it when @c per_generation_flag is set, the final gate applies
/// @c admissible to every survivor regardless of that flag, and the input
/// screen applies it once to the specification the run starts from. A property
/// enforced in only the first of those leaks through anything the search did
/// not breed -- an elite, or the seed population -- which is what made a
/// per-generation-only filter insufficient (issues #73, #77).
template <typename Spec>
struct CorrectnessCheckT {
    /// Display name, identical to the per-generation stage's. The dashboard
    /// derives its stage list from these and `run_experiments.py` parses the
    /// filter report, so a rename is a breaking change to both.
    std::string name;
    /// True when @p spec is admissible under this check.
    std::function<bool(const Spec&)> admissible;
    /// The config flag turning this check's per-generation stage on. It governs
    /// search pressure alone: the gate ignores it, so turning a check off never
    /// admits a specification that fails it.
    bool Config::* per_generation_flag = nullptr;
};

/// The FRETISH check type.
using CorrectnessCheck = CorrectnessCheckT<Specification>;

/// Returns the name of the first check @p spec fails, or nullopt if it passes
/// every one. Ordered as given, so callers pay the cheapest query first and a
/// rejected specification never reaches the expensive ones.
template <typename Spec>
std::optional<std::string> first_failing_check(
    const Spec& spec, const std::vector<CorrectnessCheckT<Spec>>& checks) {
    for (const CorrectnessCheckT<Spec>& check : checks) {
        if (!check.admissible(spec)) {
            return check.name;
        }
    }
    return std::nullopt;
}

/// What the input screen found, for the run manifest to record.
///
/// Process-global for the reason the tool-call counters are: the manifest is
/// written by a driver that no longer holds the specification, and on the TLSF
/// path the front end parses its own input, so there is no call frame holding
/// both the verdict and the writer. Set once, before the search starts.
struct InputScreen {
    /// Name of the check the run's input failed. Empty when it passed, and
    /// when no screen ran.
    inline static std::string failed_check;
};

/// The warning printed when the input specification fails @p check_name.
///
/// A warning rather than a rejection. An input that fails a check cannot itself
/// be written as a repair -- the gate rejects it -- but a descendant that
/// repairs the property can be, and the search can reach one, so refusing to
/// start would foreclose a repair the tool can express (issue #77).
std::string input_screen_warning(const std::string& check_name);

/// The FRETISH correctness checks, cheapest first.
///
/// `vacuity` leads: its syntactic screen costs nothing and its `black` queries
/// are keyed per requirement, so a candidate bred from a scored parent pays
/// only for what mutation changed. `not-well-separated` is last because it is
/// the one check that can be cold at the gate -- with its per-generation stage
/// off, nothing in the run has warmed its `ltlsynt` query, whereas
/// realizability is a scored objective and so already memoised for everything
/// the final generation scored.
///
/// @param sat  Satisfiability checker (`black`); captured by reference into the
///             returned predicates and must outlive them
/// @param real Realizability checker (`ltlsynt`); likewise
std::vector<CorrectnessCheck> correctness_checks(SatisfiabilityChecker& sat,
                                                 RealizabilityChecker& real);
