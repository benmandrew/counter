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
    const std::size_t index = random_source.next_index(atoms.size());
    // Prefer a distinct atom, as the TLSF path's flip_or_replace_atom does.
    // Without this a rename is a no-op one time in the pool size, which on a
    // two-signal specification is every other draw. The step past costs no
    // draw.
    if (atoms[index] == atom && atoms.size() > 1) {
        return atoms[(index + 1) % atoms.size()];
    }
    return atoms[index];
}

// Rename the atom, negate it, or graft a drawn anchor onto it under a fresh
// connective. The graft case is what lets a weakening guard a *positive*
// literal in one draw. Before it, the only rule that grew a formula in place
// was mutate_not_subtree's, which fires at a Not node alone, so guarding a
// negated literal cost one draw and guarding a positive one cost three
// (rename, negate, graft) through intermediates that had to survive selection
// to reach the third. experiments/2026-08-14-aurus-h2h/REPORT.md §1 measures
// the consequence: minepump's ideals guard a positive antecedent, and counter
// reached one in 1 of 20 seeds while AuRUS, whose add-disjunct rule has no
// such gate, reached it in 30 of 30.
Formula mutate_atom_formula(const Formula& formula,
                            const std::vector<std::string>& atoms,
                            const RandomSource& random_source) {
    const std::optional<std::string> atom = formula.atom_name();
    if (!atom.has_value()) {
        assert(false);
        __builtin_unreachable();
    }
    // Without a pool there is no anchor to draw, so the graft case is dropped
    // rather than guarded inside it.
    const std::size_t n_moves = atoms.empty() ? 2 : 3;
    switch (random_source.next_index(n_moves)) {
        case 0:
            return Formula::make_atom(
                mutate_atom_name(*atom, atoms, random_source));
        case 1:
            return Formula::make_unary(Formula::Kind::Not, formula);
        default: {
            // Sequenced into locals: each call draws, and the evaluation order
            // of a call's arguments is unspecified. The anchor's polarity is
            // drawn as well: a guard is as often negative as positive, and
            // minepump's ideal wants `high_water & !methane` from an atom
            // `high_water`, which a positive-only anchor cannot reach without
            // a second mutation.
            const Formula atom_formula =
                Formula::make_atom(random_atom(atoms, random_source));
            const bool negate_anchor = random_source.next_bool();
            const Formula anchor =
                negate_anchor
                    ? Formula::make_unary(Formula::Kind::Not, atom_formula)
                    : atom_formula;
            const Formula::Kind kind = pick_binary_kind(random_source);
            const bool anchor_first = random_source.next_bool();
            return anchor_first ? Formula::make_binary(kind, anchor, formula)
                                : Formula::make_binary(kind, formula, anchor);
        }
    }
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
    // Removed requirements contribute no timings: strengthening draws from the
    // shapes the specification actually uses, and a deleted requirement's is
    // not one of them.
    const auto add = [&pool](const std::vector<Requirement>& reqs) {
        for (const Requirement& req : reqs) {
            if (req.m_removed) {
                continue;
            }
            pool.push_back(req.m_timing);
        }
    };
    add(specification.m_assumptions);
    add(specification.m_guarantees);
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
// be mutated. Non-weakenable and removed requirements are excluded: rewriting a
// removed requirement spends the mutation on content nothing reads.
std::vector<std::size_t> collect_weakenable_indices(
    const Specification& specification) {
    const std::size_t n_assumptions = specification.m_assumptions.size();
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < n_assumptions; ++i) {
        const Requirement& req = specification.m_assumptions[i];
        if (req.m_weakenable && !req.m_removed) {
            indices.push_back(i);
        }
    }
    for (std::size_t i = 0; i < specification.m_guarantees.size(); ++i) {
        const Requirement& req = specification.m_guarantees[i];
        if (req.m_weakenable && !req.m_removed) {
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

// The atom pool a freshly added assumption draws its condition and response
// from. Inputs plus outputs under allow_output_assumptions (the default);
// inputs alone with it off. Historically outputs were excluded on the same
// reasoning as the trigger restriction (an output denotes the next state, so
// guarding on one
// gives the synthesiser a self-referential condition it can discharge
// vacuously), but under the flag that syntactic ban is lifted and well-
// separation is delegated to the well-separation filter, which prunes any
// assumption the system can force to fail. The draw order and count are
// identical whether or not outputs are admitted, so the flag never perturbs a
// run that leaves it off.
std::vector<std::string> assumption_atom_pool(
    const Specification& specification, const Config& cfg) {
    std::vector<std::string> pool = specification.m_in_atoms;
    if (cfg.allow_output_assumptions) {
        pool.insert(pool.end(), specification.m_out_atoms.begin(),
                    specification.m_out_atoms.end());
    }
    return pool;
}

// Draws the condition of a freshly added assumption from @p pool. `true` stays
// in the draw so the unconditional GR(1) fairness assumption G F <atom> remains
// reachable — nothing else in the operator set can produce it, since
// mutate_atom_name rewrites a `true` condition to `false` rather than to an
// atom. The negation flip applies to atoms only: a negated `true` is `false`,
// and an assumption with a false condition constrains nothing.
Formula add_assumption_condition(const std::vector<std::string>& pool,
                                 const RandomSource& random_source,
                                 double p_conditional) {
    if (random_source.next_real() >= p_conditional) {
        return Formula("true");
    }
    Formula condition =
        Formula::make_atom(pool[random_source.next_index(pool.size())]);
    if (random_source.next_bool()) {
        condition = Formula::make_unary(Formula::Kind::Not, condition);
    }
    return condition;
}

// Builds a new environment assumption over the specification's atom pool:
// `whenever <atom|true> C shall eventually satisfy <atom>` — i.e.
// G(c -> F <atom>), a conditional fairness assumption (each of condition and
// response is negated on a coin flip). By default the pool is the input atoms;
// with allow_output_assumptions it also includes outputs, in which case the
// well-separation filter (rather than a syntactic ban) is what keeps the system
// from producing a vacuously-satisfiable assumption. Appending it strengthens
// the environment, which is how the algorithm repairs unrealizability that the
// rewrite-only operators cannot reach.
Specification add_assumption(const Specification& specification,
                             const RandomSource& random_source,
                             const Config& cfg) {
    const std::vector<std::string> pool =
        assumption_atom_pool(specification, cfg);
    Formula response =
        Formula::make_atom(pool[random_source.next_index(pool.size())]);
    if (random_source.next_bool()) {
        response = Formula::make_unary(Formula::Kind::Not, response);
    }
    Formula condition = add_assumption_condition(pool, random_source,
                                                 cfg.p_conditional_assumption);
    std::vector<Requirement> assumptions = specification.m_assumptions;
    assumptions.emplace_back(std::move(condition), std::move(response),
                             timing::eventually(), ConditionType::Continual,
                             /*weakenable=*/true);
    return Specification(std::move(assumptions), specification.m_guarantees,
                         specification.m_in_atoms, specification.m_out_atoms);
}

// Guarantee slots that removal may take: weakenable, and not already removed.
std::vector<std::size_t> removable_guarantee_indices(
    const Specification& specification) {
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < specification.m_guarantees.size(); ++i) {
        const Requirement& req = specification.m_guarantees[i];
        if (req.m_weakenable && !req.m_removed) {
            indices.push_back(i);
        }
    }
    return indices;
}

// Deletes one guarantee by tombstoning it in place. The slot stays so that
// every comparison against the original keeps pairing the same requirements;
// erasing it would shift the rest and score unrelated pairs against each other.
// The mirror of add_assumption: that one strengthens the environment, this one
// weakens the system's obligations, and some repairs are only reachable by
// dropping a guarantee outright.
Specification remove_guarantee(const Specification& specification,
                               const std::vector<std::size_t>& removable,
                               const RandomSource& random_source) {
    assert(!removable.empty());
    std::vector<Requirement> guarantees = specification.m_guarantees;
    const std::size_t choice =
        removable[random_source.next_index(removable.size())];
    guarantees[choice].m_removed = true;
    return Specification(specification.m_assumptions, std::move(guarantees),
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
    // assumption is a harmless no-op. Available whenever the assumption atom
    // pool is non-empty: inputs, plus outputs when allow_output_assumptions is
    // set (so a spec with outputs but no inputs can still gain an assumption).
    const bool have_assumption_pool =
        !specification.m_in_atoms.empty() ||
        (cfg.allow_output_assumptions && !specification.m_out_atoms.empty());
    if (have_assumption_pool &&
        random_source.next_real() < cfg.p_add_assumption) {
        return add_assumption(specification, random_source, cfg);
    }
    // The mirror action: delete a guarantee. Never the last live one, since a
    // specification with nothing left to guarantee is realizable by doing
    // nothing and is no repair at all. The probability is read before the draw
    // so that a zero costs no draw at all, which is what lets a config set it
    // to 0 and reproduce a run from before the operator existed.
    if (cfg.p_remove_guarantee > 0.0) {
        const std::vector<std::size_t> removable =
            removable_guarantee_indices(specification);
        // The floor is on live guarantees, not removable ones: removing the
        // only weakenable guarantee is fine while locked ones remain.
        if (!removable.empty() && count_live(specification.m_guarantees) > 1 &&
            random_source.next_real() < cfg.p_remove_guarantee) {
            return remove_guarantee(specification, removable, random_source);
        }
    }
    std::vector<std::string> atoms;
    atoms.insert(atoms.end(), specification.m_in_atoms.begin(),
                 specification.m_in_atoms.end());
    atoms.insert(atoms.end(), specification.m_out_atoms.begin(),
                 specification.m_out_atoms.end());
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
    // but *strengthening* an assumption.
    const bool is_assumption = idx < n_assumptions;
    const Direction direction = (is_assumption && cfg.strengthen_assumptions)
                                    ? Direction::Strengthen
                                    : Direction::Weaken;
    // An existing assumption is held to the same pool rule as a freshly added
    // one: with allow_output_assumptions off it draws from inputs only, so no
    // rewrite can smuggle an output atom into the environment side and defeat
    // the input-only-by-construction well-separation the flag promises. The
    // TLSF path already gates its rewrite this way (src/tlsf/mutation.cpp);
    // this is the FRETISH half. Guarantees keep the full pool either way.
    const std::vector<std::string>& mutation_atoms =
        (is_assumption && !cfg.allow_output_assumptions)
            ? specification.m_in_atoms
            : atoms;
    if (mutation_atoms.empty()) {
        // Without atoms, mutate_formula's structural rewrites cannot draw a
        // replacement atom; leave the specification unchanged.
        return specification;
    }
    // Triggers are evaluated at the current timepoint, where the current state
    // is available through input atoms; output atoms denote the next state, so
    // letting one into a trigger produces a self-referential guard the
    // synthesiser can vacuously discharge. Draw trigger atoms from inputs only.
    // Fall back to the mutation pool when there are no inputs, since mutation
    // cannot then draw an atom for a trigger at all — and that fallback is the
    // second route an output could reach an assumption, so it falls back to the
    // gated pool rather than to the full one.
    const std::vector<std::string>& condition_atoms =
        specification.m_in_atoms.empty() ? mutation_atoms
                                         : specification.m_in_atoms;
    // Both requirement lists live behind the same mutate-then-dedup-check
    // logic; pick the target and its local index so that logic appears once.
    // Exactly one mutate_requirement call happens either way, keeping the RNG
    // draw sequence (and thus seed reproducibility) identical to the two-arm
    // form.
    std::vector<Requirement>& target = is_assumption ? assumptions : guarantees;
    const std::size_t local_idx = is_assumption ? idx : idx - n_assumptions;
    target[local_idx] =
        mutate_requirement(target[local_idx], mutation_atoms, condition_atoms,
                           direction, timing_pool, random_source, cfg);
    if (creates_duplicate(target, local_idx)) {
        return specification;
    }
    return Specification(std::move(assumptions), std::move(guarantees),
                         specification.m_in_atoms, specification.m_out_atoms);
}
