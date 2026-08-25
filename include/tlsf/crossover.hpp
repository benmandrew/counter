#pragma once

/// @file crossover.hpp
/// @brief Subformula-grafting crossover operator for tlsf::Specification.

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "tlsf/specification.hpp"

/// Grafting crossover of @p parent_a and @p parent_b, following AuRUS. The
/// parents must share identical input/output signals; if they do not, @p
/// parent_a is returned unchanged. Otherwise the result starts as a copy of
/// @p parent_a and one conjunct per side is merged: for the assumption side
/// (INITIALLY, REQUIRE, ASSUME) and again for the guarantee side (PRESET,
/// ASSERT, GUARANTEE), a live conjunct of the result and a live conjunct of
/// @p parent_b are drawn uniformly and independently from anywhere on that
/// side, and with equal probability either a temporal subformula of the first
/// is replaced by one of the second, or the two are joined under one of
/// ∧, ∨, U, W. Everything else is @p parent_a's, so the offspring keeps its
/// section shape. Deleted conjuncts take no part on either side.
///
/// A target in an initial-condition section (INITIALLY or PRESET) takes a
/// donor from its counterpart section alone, since basic TLSF requires an
/// initial condition to be propositional over one side's own signals. The
/// graft replaces the first occurrence of its site, not every one.
///
/// The assumption side has a second move, drawn first and taken instead of the
/// graft when it fires: under `tlsf.mutation.p_union_assumption` a live ASSUME
/// conjunct of @p parent_b that the offspring does not already hold is
/// appended whole. That is AuRUS's level-1 union of conjunct subsets, which the
/// graft cannot express, and it is confined to the assumption side because the
/// guarantee side pairs by position with the original.
tlsf::Specification tlsf_crossover(const tlsf::Specification& parent_a,
                                   const tlsf::Specification& parent_b,
                                   const RandomSource& random_source,
                                   const Config& cfg);
