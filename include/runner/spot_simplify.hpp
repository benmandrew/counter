#pragma once

/// @file spot_simplify.hpp
/// @brief LTL simplification in process, through the linked libspot, in place
///        of spawning `ltlfilt --simplify`.
///
/// Simplification is the one SPOT tool this can be done for. It has no timeout
/// to give up: `ltl2tgba` and `ltlsynt` are given per-call deadlines that work
/// by killing a separate process, and in process there is nothing to kill.
/// Simplification never had a deadline, so moving it in process gives up
/// nothing that existed.

#include <optional>
#include <string>

/// Simplifies @p formula, returning std::nullopt when it does not parse -- the
/// caller should then leave the formula alone, as the exec path does.
///
/// The result is byte-identical to `ltlfilt --simplify`, which is
/// simplification level 3 rather than the library default; the two disagree on
/// about 5% of formulae, so the level is not a detail that can be left to the
/// default.
///
/// Callable from any thread, but every call is serialised behind one
/// process-wide mutex, and one shared simplifier is constructed under that same
/// mutex. That is not caution, it is the measured requirement: the contended
/// state is not in `tl_simplifier` at all but process-global underneath it
/// (SPOT's Bison/Flex parser globals, and the `robin_hood` table interning
/// formula nodes), so a per-thread simplifier still reaches all of it and
/// crashes -- even when every call is locked, because construction alone is
/// unsafe. See PROFILING.md.
///
/// Serialising costs nothing at this scale: a call is about 0.02 ms, against
/// the roughly 8 ms of process startup it replaces.
std::optional<std::string> spot_simplify(const std::string& formula);
