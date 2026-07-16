#pragma once

/// @file parser.hpp
/// @brief Recursive-descent parser for basic-format TLSF (Temporal Logic
///        Synthesis Format) into a tlsf::Specification.

#include <cstddef>
#include <string>

#include "tlsf/specification.hpp"

namespace tlsf {

/// Maximum expansion length for a bounded/timed operator (`X[n]`, `F[a..b]`,
/// `G[a..b]`). A bound above this throws rather than blowing up the AST.
inline constexpr std::size_t k_max_bound_expansion = 64;

/// Parses basic-TLSF source text into a Specification.
///
/// Accepts the `INFO { ... } MAIN { ... }` structure of basic TLSF. INFO
/// entries (TITLE, DESCRIPTION, SEMANTICS, TARGET, and any ignored extras) are
/// `KEY: value` with no `;` terminator, per the TLSF grammar. MAIN carries the
/// INPUTS/OUTPUTS lists plus the specification subsections, using the TLSF v1.1
/// names or their v1.0 aliases: INITIALLY, PRESET, REQUIRE/REQUIREMENTS,
/// ASSUME/ASSUMPTIONS, ASSERT/INVARIANTS, GUARANTEE/GUARANTEES. Boolean
/// connectives are `&&`/`||` (single `&`/`|` also accepted); bounded operators
/// are expanded into X-chains at parse time.
///
/// Full-format constructs (GLOBAL/PARAMETERS/DEFINITIONS blocks, bus and
/// enumeration declarations, loop aggregates, primed/bus-access syntax) are
/// rejected with a message pointing at `syfco -f basic` pre-lowering.
///
/// @param text TLSF source text.
/// @return The parsed specification.
/// @throws std::invalid_argument on malformed or unsupported input.
Specification parse(const std::string& text);

}  // namespace tlsf
