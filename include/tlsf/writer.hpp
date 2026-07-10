#pragma once

/// @file writer.hpp
/// @brief Pretty-printer that renders a tlsf::Specification back to valid
///        basic-format TLSF text.

#include <string>

#include "tlsf/specification.hpp"

namespace tlsf {

/// Serialises a Specification to basic-TLSF source text: an INFO block (TITLE,
/// DESCRIPTION, SEMANTICS, TARGET) and a MAIN block (INPUTS, OUTPUTS, then each
/// non-empty section with its formulae as `;`-terminated lines). The output
/// round-trips: `parse(write(spec))` reproduces `spec`.
std::string write(const Specification& specification);

}  // namespace tlsf
