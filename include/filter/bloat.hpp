#pragma once

/// @file bloat.hpp
/// @brief Filter that drops specifications whose formulae exceed a size ratio
///        relative to the original, preventing bloat during evolution.

#include "genetic/generation.hpp"

/// Returns a FilterFunction that drops specifications containing any single
/// formula (condition or response) larger than @p max_ratio times the largest
/// formula in @p original.
///
/// Each condition and response in every requirement is checked individually.
/// Capping per-formula rather than per-specification prevents a bloated
/// formula in one requirement from escaping detection by being diluted by
/// simple formulas elsewhere in the spec. If the original's largest formula
/// has zero subformulae (degenerate), all candidates are admitted.
///
/// @param original   The reference specification; the baseline is its largest
///                   individual formula (by n_subformulae())
/// @param max_ratio  Caps each candidate formula at max_ratio * original_max.
///                   The default of 2 is a heuristic: it admits the doubling a
///                   single crossover can cause while rejecting sustained
///                   growth across generations.
FilterFunction make_bloat_cap_filter(const Specification& original,
                                     double max_ratio = 2.0);
