#pragma once

/// @file version.hpp
/// @brief The git commit the binary was built from, for run provenance.

#include <iosfwd>

namespace version {

/// Full 40-character commit hash, or "unknown" outside a git work tree.
const char* commit();

/// Abbreviated commit hash, or "unknown" outside a git work tree.
const char* commit_short();

/// True when tracked files differed from HEAD at build time.
///
/// Untracked files do not count: they are not compiled into anything, so they
/// cannot explain a binary that disagrees with its commit.
bool dirty();

/// Writes the provenance as `key=value` lines, one per key, terminated by a
/// newline. Machine-readable on purpose — scripts/run_experiments.py parses it
/// to record what the binary was built from and to refuse a stale one.
void print(std::ostream& out);

}  // namespace version
