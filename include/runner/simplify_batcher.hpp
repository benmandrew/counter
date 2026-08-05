#pragma once

/// @file simplify_batcher.hpp
/// @brief Coalesces concurrent LTL simplification misses into one `ltlfilt`
///        exec, so a batch pays the per-process startup once instead of once
///        per formula.
///
/// Separate from ltlfilt.cpp because the two are separable concerns: that file
/// owns the cache and the fallback, this one runs a leader election over a pipe
/// pair. Keeping both in one translation unit had taken it to the
/// cognitive-complexity limit the project sets for itself.

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

/// Sets how many batches may run at once, from Config::ltlfilt_batchers. Zero
/// disables batching, giving every call its own exec.
///
/// Call once at startup, before any scoring thread runs. The pool is built on
/// first use and never resized, because scoring threads index into it without
/// a lock and growing it underneath them would be a race.
void set_ltlfilt_batchers(std::size_t count);

/// Simplifies @p formula as part of a batch, blocking until it is resolved.
///
/// Returns std::nullopt to mean "run it yourself": batching is off, the
/// formula cannot be batched, or the batch failed its line-count check. A
/// caller that gets nothing back is expected to fall back to its own exec
/// rather than treat it as a failed simplification.
///
/// @p child_cpu_s receives the whole batch's child CPU when this caller ran the
/// batch, and is left alone when another caller did, so the total is counted
/// once.
std::optional<std::string> batched_simplify(const std::string& binary,
                                            const std::string& formula,
                                            double& child_cpu_s,
                                            std::chrono::milliseconds timeout);
