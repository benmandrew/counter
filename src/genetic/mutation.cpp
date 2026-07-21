#include "genetic/mutation.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "prop_formula.hpp"

namespace {

Formula::Kind pick_binary_kind(const RandomSource& random_source) {
    const int selector = static_cast<int>(random_source.next_index(4));
    switch (selector) {
        case 0:
            return Formula::Kind::And;
        case 1:
            return Formula::Kind::Or;
        case 2:
            return Formula::Kind::Implies;
        case 3:
            return Formula::Kind::Iff;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

std::string random_atom(const std::vector<std::string>& atoms,
                        const RandomSource& random_source) {
    assert(!atoms.empty());
    return atoms[random_source.next_index(atoms.size())];
}

std::string mutate_atom_name(const std::string& atom,
                             const std::vector<std::string>& atoms,
                             const RandomSource& random_source) {
    if (atom == "true") {
        return "false";
    }
    if (atom == "false") {
        return "true";
    }
    if (atoms.empty()) {
        return atom;
    }
    return atoms[random_source.next_index(atoms.size())];
}

Formula mutate_atom_formula(const Formula& formula,
                            const std::vector<std::string>& atoms,
                            const RandomSource& random_source) {
    const std::optional<std::string> atom = formula.atom_name();
    if (!atom.has_value()) {
        assert(false);
        __builtin_unreachable();
    }
    if (random_source.next_bool()) {
        return Formula::make_atom(
            mutate_atom_name(*atom, atoms, random_source));
    }
    return Formula::make_unary(Formula::Kind::Not, formula);
}

Formula mutate_not_subtree(Formula child, const std::vector<std::string>& atoms,
                           const RandomSource& random_source) {
    const int selector = static_cast<int>(random_source.next_index(3));
    switch (selector) {
        case 0:
            return child;
        case 1:
            return Formula::make_unary(Formula::Kind::Not, child);
        case 2:
            break;
        default:
            assert(false);
            __builtin_unreachable();
    }
    const Formula anchor =
        Formula::make_atom(random_atom(atoms, random_source));
    return Formula::make_binary(pick_binary_kind(random_source), anchor,
                                Formula::make_unary(Formula::Kind::Not, child));
}

Formula mutate_binary_subtree(const std::pair<Formula, Formula>& children,
                              const RandomSource& random_source) {
    if (!random_source.next_bool()) {
        return random_source.next_bool() ? children.first : children.second;
    }
    Formula combined = Formula::make_binary(pick_binary_kind(random_source),
                                            children.first, children.second);
    if (!random_source.next_bool()) {
        return combined;
    }
    return Formula::make_unary(Formula::Kind::Not, combined);
}

}  // namespace

Formula mutate_formula(const Formula& formula,
                       const std::vector<std::string>& atoms,
                       const RandomSource& random_source) {
    assert(random_source);
    const std::size_t n_subformulas = formula.n_subformulae();
    const auto mutation_function =
        [&](const Formula& subtree) -> std::optional<Formula> {
        if (random_source.next_index(n_subformulas) != 0) {
            return std::nullopt;
        }
        switch (subtree.kind()) {
            case Formula::Kind::Atom:
                return mutate_atom_formula(subtree, atoms, random_source);
            case Formula::Kind::Not: {
                auto child_opt = subtree.unary_child();
                if (!child_opt.has_value()) {
                    assert(false);
                    __builtin_unreachable();
                }
                return mutate_not_subtree(*child_opt, atoms, random_source);
            }
            case Formula::Kind::And:
            case Formula::Kind::Or:
            case Formula::Kind::Implies:
            case Formula::Kind::Iff: {
                const auto children_opt = subtree.binary_children();
                if (!children_opt.has_value()) {
                    assert(false);
                    __builtin_unreachable();
                }
                return mutate_binary_subtree(*children_opt, random_source);
            }
            // FRETISH formulae are propositional; temporal operators never
            // occur here. Leave any such subtree unmutated.
            case Formula::Kind::Next:
            case Formula::Kind::Eventually:
            case Formula::Kind::Globally:
            case Formula::Kind::Until:
            case Formula::Kind::Release:
            case Formula::Kind::WeakUntil:
                return std::nullopt;
        }
        return std::nullopt;
    };
    auto mutated = formula.rewrite_post_order(mutation_function);
    mutated.remove_double_negation();
    return mutated;
}

namespace {

Timing weaken_for_timing(const timing::ForTicks& for_ticks,
                         const RandomSource& random_source) {
    if (for_ticks.m_ticks == 1) {
        return random_source.next_bool() ? timing::next_timepoint()
                                         : timing::immediately();
    }
    return random_source.next_bool() ? timing::for_ticks(for_ticks.m_ticks - 1)
                                     : timing::for_ticks(for_ticks.m_ticks / 2);
}

Timing weaken_within_timing(const timing::WithinTicks& within_ticks,
                            const RandomSource& random_source) {
    std::size_t index = random_source.next_index(3);
    switch (index) {
        case 0:
            return timing::within_ticks(within_ticks.m_ticks + 1);
        case 1:
            return timing::within_ticks(within_ticks.m_ticks * 2);
        case 2:
            return timing::eventually();
        default:
            assert(false);
            __builtin_unreachable();
    }
}

Timing strengthen_for_timing(const timing::ForTicks& for_ticks,
                             const RandomSource& random_source) {
    std::size_t index = random_source.next_index(3);
    switch (index) {
        case 0:
            return timing::for_ticks(for_ticks.m_ticks + 1);
        case 1:
            return timing::for_ticks(for_ticks.m_ticks * 2);
        case 2:
            return timing::always();
        default:
            assert(false);
            __builtin_unreachable();
    }
}

Timing strengthen_within_timing(const timing::WithinTicks& within_ticks,
                                const RandomSource& random_source) {
    if (within_ticks.m_ticks == 1) {
        return random_source.next_bool() ? timing::next_timepoint()
                                         : timing::immediately();
    }
    std::size_t index = random_source.next_index(3);
    switch (index) {
        case 0:
            return timing::within_ticks(within_ticks.m_ticks - 1);
        case 1:
            // Halve rounding up, so the result stays >= 1 and strictly below
            // m_ticks for every m_ticks > 1.
            return timing::within_ticks((within_ticks.m_ticks + 1) / 2);
        case 2:
            return timing::after_ticks(within_ticks.m_ticks - 1);
        default:
            assert(false);
            __builtin_unreachable();
    }
}

// Timing is an alias for a std::variant, so ADL from inside <algorithm> finds
// only std's element-wise variant comparisons, not requirement.hpp's
// namespace-scope ones. Naming them in a lambda picks up the right overloads.
void sort_unique_timings(std::vector<Timing>& timings) {
    std::sort(timings.begin(), timings.end(),
              [](const Timing& lhs, const Timing& rhs) { return lhs < rhs; });
    timings.erase(std::unique(timings.begin(), timings.end(),
                              [](const Timing& lhs, const Timing& rhs) {
                                  return lhs == rhs;
                              }),
                  timings.end());
}

// The strengthenings Eventually may take, derived from the timings the
// specification already uses. A quantified donor lends only its tick count,
// spent as `for n ticks`; Immediately and NextTimepoint lend themselves.
std::vector<Timing> eventually_candidates(
    const std::vector<Timing>& timing_pool) {
    std::vector<Timing> candidates;
    for (const Timing& donor : timing_pool) {
        std::visit(
            [&candidates](const auto& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, timing::WithinTicks> ||
                              std::is_same_v<T, timing::ForTicks> ||
                              std::is_same_v<T, timing::AfterTicks>) {
                    // `for 0 ticks` is just Immediately spelled differently,
                    // and only AfterTicks can carry a zero count.
                    if (value.m_ticks > 0) {
                        candidates.push_back(timing::for_ticks(value.m_ticks));
                    }
                } else if constexpr (std::is_same_v<T, timing::Immediately>) {
                    candidates.push_back(timing::immediately());
                } else if constexpr (std::is_same_v<T, timing::NextTimepoint>) {
                    candidates.push_back(timing::next_timepoint());
                }
            },
            donor);
    }
    sort_unique_timings(candidates);
    return candidates;
}

Timing strengthen_eventually(const std::vector<Timing>& timing_pool,
                             const RandomSource& random_source) {
    const std::vector<Timing> candidates = eventually_candidates(timing_pool);
    if (candidates.empty()) {
        return timing::eventually();
    }
    return candidates[random_source.next_index(candidates.size())];
}

Timing strengthen_timing(const Timing& timing,
                         const std::vector<Timing>& timing_pool,
                         const RandomSource& random_source) {
    const auto mutation_function = [&](const auto& value) -> Timing {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, timing::Immediately> ||
                      std::is_same_v<T, timing::NextTimepoint>) {
            return timing::for_ticks(1);
        } else if constexpr (std::is_same_v<T, timing::Always>) {
            // Always is the top of the order and has no strengthening.
            return timing::always();
        } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
            return strengthen_for_timing(value, random_source);
        } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
            // `after n` expands to ¬R at ticks 0..n ∧ R at tick n+1, pinning
            // the response to a single tick. Nothing in the timing order lies
            // strictly above it: `after n-1` and `always` both demand R at a
            // tick where `after n` demands ¬R, so they contradict rather than
            // strengthen it. AfterTicks therefore has no strengthening.
            return timing::after_ticks(value.m_ticks);
        } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
            return strengthen_within_timing(value, random_source);
        } else if constexpr (std::is_same_v<T, timing::Eventually>) {
            return strengthen_eventually(timing_pool, random_source);
        } else {
            assert(false);
            __builtin_unreachable();
        }
    };
    return std::visit(mutation_function, timing);
}

Timing weaken_timing(const Timing& timing, const RandomSource& random_source) {
    const auto mutation_function = [&](const auto& value) -> Timing {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, timing::Immediately> ||
                      std::is_same_v<T, timing::NextTimepoint>) {
            return timing::within_ticks(1);
        } else if constexpr (std::is_same_v<T, timing::Always>) {
            // Always must not be weakened.
            return timing::always();
        } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
            return weaken_for_timing(value, random_source);
        } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
            return timing::within_ticks(value.m_ticks + 1);
        } else if constexpr (std::is_same_v<T, timing::WithinTicks>) {
            return weaken_within_timing(value, random_source);
        } else if constexpr (std::is_same_v<T, timing::Eventually>) {
            return timing::eventually();
        } else {
            assert(false);
            __builtin_unreachable();
        }
    };
    return std::visit(mutation_function, timing);
}

}  // namespace

std::vector<Timing> collect_timing_pool(const Specification& specification) {
    std::vector<Timing> pool;
    pool.reserve(specification.m_assumptions.size() +
                 specification.m_guarantees.size());
    for (const Requirement& req : specification.m_assumptions) {
        pool.push_back(req.m_timing);
    }
    for (const Requirement& req : specification.m_guarantees) {
        pool.push_back(req.m_timing);
    }
    sort_unique_timings(pool);
    return pool;
}

Timing mutate_timing(const Timing& timing, Direction direction,
                     const std::vector<Timing>& timing_pool,
                     const RandomSource& random_source) {
    assert(random_source);
    return direction == Direction::Strengthen
               ? strengthen_timing(timing, timing_pool, random_source)
               : weaken_timing(timing, random_source);
}

Requirement mutate_requirement(const Requirement& requirement,
                               const std::vector<std::string>& atoms,
                               const std::vector<std::string>& condition_atoms,
                               Direction direction,
                               const std::vector<Timing>& timing_pool,
                               const RandomSource& random_source,
                               const Config& cfg) {
    Requirement mutated = requirement;
    // Response and condition mutation is still direction-agnostic: it rewrites
    // the propositional structure freely and relies on the population filters
    // to discard candidates that moved the wrong way.
    if (random_source.next_real() < cfg.p_response) {
        mutated.m_response =
            mutate_formula(requirement.m_response, atoms, random_source);
    }
    if (random_source.next_real() < cfg.p_trigger) {
        mutated.m_condition = mutate_formula(requirement.m_condition,
                                             condition_atoms, random_source);
    }
    if (random_source.next_real() < cfg.p_timing) {
        mutated.m_timing = mutate_timing(requirement.m_timing, direction,
                                         timing_pool, random_source);
    }
    mutated.m_ltl = requirement_to_ltl(mutated);
    return mutated;
}

namespace {

// Global indices (assumptions first, then guarantees) of requirements that may
// be mutated. Non-weakenable requirements are excluded.
std::vector<std::size_t> collect_weakenable_indices(
    const Specification& specification) {
    const std::size_t n_assumptions = specification.m_assumptions.size();
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < n_assumptions; ++i) {
        if (specification.m_assumptions[i].m_weakenable) {
            indices.push_back(i);
        }
    }
    for (std::size_t i = 0; i < specification.m_guarantees.size(); ++i) {
        if (specification.m_guarantees[i].m_weakenable) {
            indices.push_back(n_assumptions + i);
        }
    }
    return indices;
}

// True if the requirement at @p idx is equal to any other requirement in the
// list, using the same ordering-based equality as the rest of the algorithm.
bool creates_duplicate(const std::vector<Requirement>& requirements,
                       std::size_t idx) {
    for (std::size_t i = 0; i < requirements.size(); ++i) {
        if (i == idx) {
            continue;
        }
        const bool equal = !(requirements[i] < requirements[idx]) &&
                           !(requirements[idx] < requirements[i]);
        if (equal) {
            return true;
        }
    }
    return false;
}

// Draws the condition of a freshly added assumption. Input atoms only, for the
// same reason mutate_specification restricts trigger atoms: an output denotes
// the next state, so guarding on one gives the synthesiser a self-referential
// condition it can discharge vacuously. `true` stays in the draw so the
// unconditional GR(1) fairness assumption G F <input> remains reachable —
// nothing else in the operator set can produce it, since mutate_atom_name
// rewrites a `true` condition to `false` rather than to an atom. The negation
// flip applies to atoms only: a negated `true` is `false`, and an assumption
// with a false condition constrains nothing.
Formula add_assumption_condition(const std::vector<std::string>& inputs,
                                 const RandomSource& random_source,
                                 double p_conditional) {
    if (random_source.next_real() >= p_conditional) {
        return Formula("true");
    }
    Formula condition =
        Formula::make_atom(inputs[random_source.next_index(inputs.size())]);
    if (random_source.next_bool()) {
        condition = Formula::make_unary(Formula::Kind::Not, condition);
    }
    return condition;
}

// Builds a new environment assumption over the specification's input atoms:
// `whenever <input|true> C shall eventually satisfy <input>` — i.e.
// G(c -> F <input>), a conditional fairness assumption (each of condition and
// response is negated on a coin flip). Appending it strengthens the
// environment, which is how the algorithm repairs unrealizability that the
// rewrite-only operators cannot reach.
Specification add_assumption(const Specification& specification,
                             const RandomSource& random_source,
                             const Config& cfg) {
    const std::string& atom = specification.m_in_atoms[random_source.next_index(
        specification.m_in_atoms.size())];
    Formula response = Formula::make_atom(atom);
    if (random_source.next_bool()) {
        response = Formula::make_unary(Formula::Kind::Not, response);
    }
    Formula condition = add_assumption_condition(
        specification.m_in_atoms, random_source, cfg.p_conditional_assumption);
    std::vector<Requirement> assumptions = specification.m_assumptions;
    assumptions.emplace_back(std::move(condition), std::move(response),
                             timing::eventually(), ConditionType::Continual,
                             /*weakenable=*/true);
    return Specification(std::move(assumptions), specification.m_guarantees,
                         specification.m_in_atoms, specification.m_out_atoms);
}

}  // namespace

Specification mutate_specification(const Specification& specification,
                                   const RandomSource& random_source,
                                   const Config& cfg) {
    assert(random_source);
    const std::size_t n_assumptions = specification.m_assumptions.size();
    assert(n_assumptions + specification.m_guarantees.size() > 0);
    // Low-probability structural action: add a new environment assumption. The
    // Specification constructor deduplicates, so re-adding an existing
    // assumption is a harmless no-op.
    if (!specification.m_in_atoms.empty() &&
        random_source.next_real() < cfg.p_add_assumption) {
        return add_assumption(specification, random_source, cfg);
    }
    std::vector<std::string> atoms;
    atoms.insert(atoms.end(), specification.m_in_atoms.begin(),
                 specification.m_in_atoms.end());
    atoms.insert(atoms.end(), specification.m_out_atoms.begin(),
                 specification.m_out_atoms.end());
    // Triggers are evaluated at the current timepoint, where the current state
    // is available through input atoms; output atoms denote the next state, so
    // letting one into a trigger produces a self-referential guard the
    // synthesiser can vacuously discharge. Draw trigger atoms from inputs only.
    // Fall back to the full pool when there are no inputs, since mutation
    // cannot then draw an atom for a trigger at all.
    const std::vector<std::string>& condition_atoms =
        specification.m_in_atoms.empty() ? atoms : specification.m_in_atoms;
    const std::vector<Timing> timing_pool = collect_timing_pool(specification);
    const std::vector<std::size_t> weakenable_indices =
        collect_weakenable_indices(specification);
    if (weakenable_indices.empty()) {
        return specification;
    }
    const std::size_t idx =
        weakenable_indices[random_source.next_index(weakenable_indices.size())];
    std::vector<Requirement> assumptions = specification.m_assumptions;
    std::vector<Requirement> guarantees = specification.m_guarantees;
    // Weakening the assume-guarantee specification means weakening a guarantee
    // but *strengthening* an assumption: a stricter environment expands the set
    // of environments under which the guarantees must hold.
    const Direction direction =
        (idx < n_assumptions && cfg.strengthen_assumptions)
            ? Direction::Strengthen
            : Direction::Weaken;
    if (idx < n_assumptions) {
        assumptions[idx] =
            mutate_requirement(assumptions[idx], atoms, condition_atoms,
                               direction, timing_pool, random_source, cfg);
        if (creates_duplicate(assumptions, idx)) {
            return specification;
        }
    } else {
        const std::size_t g_idx = idx - n_assumptions;
        guarantees[g_idx] =
            mutate_requirement(guarantees[g_idx], atoms, condition_atoms,
                               direction, timing_pool, random_source, cfg);
        if (creates_duplicate(guarantees, g_idx)) {
            return specification;
        }
    }
    return Specification(std::move(assumptions), std::move(guarantees),
                         specification.m_in_atoms, specification.m_out_atoms);
}
