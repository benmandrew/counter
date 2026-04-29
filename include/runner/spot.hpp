#pragma once

#include <string>

#include "requirement.hpp"

std::string spot_bin_dir();

std::string ltlsynt_path();

/// Returns true if the requirement's LTL specification is realisable.
/// Trigger atoms are treated as environment inputs; response atoms as system
/// outputs. Atoms appearing in both are classified as outputs.
bool check_realizability(const Requirement& requirement);
