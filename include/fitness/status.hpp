#pragma once

/// @file status.hpp
/// @brief Realizability status fitness score, on the three-point scale shared
///        by the FRETISH and TLSF front ends.

#include <functional>
#include <string>
#include <vector>

#include "requirement.hpp"
#include "runner/black.hpp"
#include "runner/spot.hpp"

/// Some component formula of the candidate is unsatisfiable on its own.
inline constexpr double k_status_component_unsatisfiable = 0.0;
/// Every component is satisfiable, but the candidate is unrealizable.
inline constexpr double k_status_unrealizable = 0.5;
/// The candidate is realizable.
inline constexpr double k_status_realizable = 1.0;

/// Scores a candidate on the shared status scale, given its component formulae
/// and a way to decide realizability. Both front ends route through this, so
/// the scale lives in one place; only the decomposition into components and
/// the realizability query differ between them.
///
/// The scale is deliberately coarse. Earlier revisions graded the region below
/// realizability more finely -- separate tiers for an unsatisfiable guarantee
/// side and an unsatisfiable assumption side, mirroring the design this search
/// is derived from. Instrumenting both front ends over 887k scored candidates
/// found those tiers hold under 0.32% of the population between them.
///
/// The two failed for different reasons. The guarantee-side tier was measured:
/// nothing filters a jointly unsatisfiable guarantee side, and it fired on 46
/// candidates out of 15,115 on the TLSF path, all on one specification. The
/// assumption-side tier never fired at all, and could not have: its predicate
/// is exactly the vacuity filter's, and candidate filters run on offspring
/// *before* those offspring are scored, so such a candidate is discarded
/// before it reaches this function. The tier was unreachable in any run
/// leaving run_vacuity_filter on, which is the default -- it re-asked a
/// question a filter had already acted on.
///
/// Grading *within* unrealizability needs a measure of how far a candidate is
/// from realizable, which a satisfiability query cannot express.
///
/// @param components    Formulae to test individually; any one unsatisfiable
///                      scores k_status_component_unsatisfiable
/// @param sat           Satisfiability checker (black)
/// @param is_realizable Consulted only once every component is satisfiable, so
///                      a caller may fold in a cheap shortcut for candidates
///                      whose realizability is known without a solver call
/// @return              One of the three k_status_* constants
double status_score(const std::vector<std::string>& components,
                    SatisfiabilityChecker& sat,
                    const std::function<bool()>& is_realizable);

/// Status score of a FRETISH specification. Its components are the
/// per-requirement conjunctions `condition & response`: a requirement is
/// incoherent when its own condition and response cannot hold together, which
/// already covers either half being unsatisfiable alone. Requirements are
/// checked separately rather than conjoined across the specification, since
/// they fire at different times and need not hold simultaneously.
///
/// @param specification A specification whose requirements all have m_ltl set
/// @param sat           Satisfiability checker (black)
/// @param real          Realizability checker (ltlsynt)
double specification_status(const Specification& specification,
                            SatisfiabilityChecker& sat,
                            RealizabilityChecker& real);
