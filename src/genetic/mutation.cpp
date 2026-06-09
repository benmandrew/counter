#include "genetic/mutation.hpp"

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
    }
    assert(false);
    __builtin_unreachable();
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
    assert(atom.has_value());
    if (random_source.next_bool()) {
        return Formula::make_atom(
            mutate_atom_name(*atom, atoms, random_source));
    }
    return Formula::make_unary(Formula::Kind::Not, formula);
}

}  // namespace

Formula mutate_formula(const Formula& formula,
                       const std::vector<std::string>& atoms,
                       const RandomSource& random_source) {
    assert(random_source);
    const std::size_t n = formula.n_subformulae();
    const auto mutation_function =
        [&](const Formula& subtree) -> std::optional<Formula> {
        if (random_source.next_index(n) != 0) {
            return std::nullopt;
        }
        switch (subtree.kind()) {
            case Formula::Kind::Atom:
                return mutate_atom_formula(subtree, atoms, random_source);
            case Formula::Kind::Not: {
                const Formula child = subtree.unary_child().value();
                const int selector =
                    static_cast<int>(random_source.next_index(4));
                switch (selector) {
                    case 0:
                        return child;
                    case 1:
                        return Formula::make_unary(Formula::Kind::Not, child);
                    case 2:
                        return Formula::make_unary(
                            Formula::Kind::Not,
                            Formula::make_unary(Formula::Kind::Not, child));
                    case 3:
                        break;
                }
                const Formula anchor =
                    Formula::make_atom(random_atom(atoms, random_source));
                return Formula::make_binary(
                    pick_binary_kind(random_source), anchor,
                    Formula::make_unary(Formula::Kind::Not, child));
            }
            case Formula::Kind::And:
            case Formula::Kind::Or:
            case Formula::Kind::Implies:
            case Formula::Kind::Iff: {
                const auto children = subtree.binary_children().value();
                if (!random_source.next_bool()) {
                    return random_source.next_bool() ? children.first
                                                     : children.second;
                }
                const Formula combined =
                    Formula::make_binary(pick_binary_kind(random_source),
                                         children.first, children.second);
                if (!random_source.next_bool()) {
                    return combined;
                }
                return Formula::make_unary(Formula::Kind::Not, combined);
            }
        }
        return std::nullopt;
    };
    auto f = formula.rewrite_post_order(mutation_function);
    f.remove_double_negation();
    return f;
}

Timing mutate_timing(const Timing& timing, const RandomSource& random_source) {
    assert(random_source);
    const auto mutation_function = [&](const auto& value) -> Timing {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, timing::Immediately> ||
                      std::is_same_v<T, timing::NextTimepoint>) {
            return timing::within_ticks(1);
        } else if constexpr (std::is_same_v<T, timing::Eventually>) {
            return timing::eventually();
        } else if constexpr (std::is_same_v<T, timing::AfterTicks>) {
            return timing::within_ticks(value.m_ticks + 1);
        } else if constexpr (std::is_same_v<T, timing::ForTicks>) {
            if (value.m_ticks == 1) {
                return random_source.next_bool() ? timing::next_timepoint()
                                                 : timing::immediately();
            }
            return random_source.next_bool()
                       ? timing::for_ticks(value.m_ticks - 1)
                       : timing::for_ticks(value.m_ticks / 2);
        } else {
            return random_source.next_bool()
                       ? timing::within_ticks(value.m_ticks + 1)
                       : timing::within_ticks(value.m_ticks * 2);
        }
    };
    return std::visit(mutation_function, timing);
}

Requirement mutate_requirement(const Requirement& requirement,
                               const std::vector<std::string>& atoms,
                               const RandomSource& random_source) {
    Requirement mutated = requirement;
    mutated.m_response =
        mutate_formula(requirement.m_response, atoms, random_source);
    mutated.m_trigger =
        mutate_formula(requirement.m_trigger, atoms, random_source);
    mutated.m_timing = mutate_timing(requirement.m_timing, random_source);
    return mutated;
}

Specification mutate_specification(const Specification& specification,
                                   const RandomSource& random_source) {
    assert(random_source);
    const std::size_t n_assumptions = specification.m_assumptions.size();
    const std::size_t n_guarantees = specification.m_guarantees.size();
    assert(n_assumptions + n_guarantees > 0);
    std::vector<std::string> atoms;
    atoms.insert(atoms.end(), specification.m_in_atoms.begin(),
                 specification.m_in_atoms.end());
    atoms.insert(atoms.end(), specification.m_out_atoms.begin(),
                 specification.m_out_atoms.end());
    const std::size_t idx =
        random_source.next_index(n_assumptions + n_guarantees);
    std::vector<Requirement> assumptions = specification.m_assumptions;
    std::vector<Requirement> guarantees = specification.m_guarantees;
    if (idx < n_assumptions) {
        assumptions[idx] =
            mutate_requirement(assumptions[idx], atoms, random_source);
        for (std::size_t i = 0; i < assumptions.size(); ++i) {
            if (i != idx) {
                const bool equal = !(assumptions[i] < assumptions[idx]) &&
                                   !(assumptions[idx] < assumptions[i]);
                if (equal) return specification;
            }
        }
    } else {
        const std::size_t g_idx = idx - n_assumptions;
        guarantees[g_idx] =
            mutate_requirement(guarantees[g_idx], atoms, random_source);
        for (std::size_t i = 0; i < guarantees.size(); ++i) {
            if (i != g_idx) {
                const bool equal = !(guarantees[i] < guarantees[g_idx]) &&
                                   !(guarantees[g_idx] < guarantees[i]);
                if (equal) return specification;
            }
        }
    }
    return Specification(std::move(assumptions), std::move(guarantees),
                         specification.m_in_atoms, specification.m_out_atoms);
}
