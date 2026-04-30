#include "genetic/mutation.hpp"

#include <cstdlib>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

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
    throw std::logic_error("Failed to select a binary operator kind.");
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
        throw std::logic_error("Expected atomic formula for atom mutation.");
    }

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
    if (!random_source) {
        throw std::invalid_argument("random_source must be callable.");
    }
    const auto mutation_function =
        [&](const Formula& subtree) -> std::optional<Formula> {
        if (!random_source.next_bool()) {
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
                const Formula anchor = Formula::make_atom("p_mut");
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
    return formula.rewrite_post_order(mutation_function);
}

Timing pick_non_parameter_timing(const RandomSource& random_source) {
    return random_source.next_bool() ? timing::immediately()
                                     : timing::next_timepoint();
}

Timing pick_parameter_timing(std::size_t ticks,
                             const RandomSource& random_source) {
    return random_source.next_bool() ? timing::within_ticks(ticks)
                                     : timing::for_ticks(ticks);
}

std::size_t pick_tick_count(const RandomSource& random_source) {
    return random_source.next_index(8) + 1;
}

std::size_t mutate_tick_count(std::size_t ticks,
                              const RandomSource& random_source) {
    const std::size_t candidate = pick_tick_count(random_source);
    if (candidate != ticks) {
        return candidate;
    }
    if (ticks < std::numeric_limits<std::size_t>::max()) {
        return ticks + 1;
    }
    return ticks - 1;
}

Timing mutate_timing(const Timing& timing, const RandomSource& random_source) {
    if (!random_source) {
        throw std::invalid_argument("random_source must be callable.");
    }
    const auto mutation_function = [&](const auto& value) -> Timing {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, timing::Immediately> ||
                      std::is_same_v<T, timing::NextTimepoint>) {
            if (!random_source.next_bool()) {
                // Replace with the other non-parameterized timing.
                if constexpr (std::is_same_v<T, timing::Immediately>) {
                    return timing::next_timepoint();
                } else {
                    return timing::immediately();
                }
            }
            // Replace with a parameterized timing.
            return pick_parameter_timing(pick_tick_count(random_source),
                                         random_source);
        } else {
            if (!random_source.next_bool()) {
                // Change operator only, preserve parameter.
                if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                    return timing::for_ticks(value.m_ticks);
                } else {
                    return timing::within_ticks(value.m_ticks);
                }
            }
            if (!random_source.next_bool()) {
                // Change parameter only, preserve operator.
                const std::size_t mutated_ticks =
                    mutate_tick_count(value.m_ticks, random_source);
                if constexpr (std::is_same_v<T, timing::WithinTicks>) {
                    return timing::within_ticks(mutated_ticks);
                } else {
                    return timing::for_ticks(mutated_ticks);
                }
            }
            // Replace with a non-parameterized timing.
            return pick_non_parameter_timing(random_source);
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
    if (!random_source) {
        throw std::invalid_argument("random_source must be callable.");
    }
    if (specification.m_requirements.empty()) {
        throw std::invalid_argument(
            "Specification must not be empty to mutate.");
    }
    std::vector<std::string> atoms;
    atoms.insert(atoms.end(), specification.m_in_atoms.begin(),
                 specification.m_in_atoms.end());
    atoms.insert(atoms.end(), specification.m_out_atoms.begin(),
                 specification.m_out_atoms.end());
    std::vector<Requirement> reqs(specification.m_requirements.begin(),
                                  specification.m_requirements.end());
    const std::size_t idx = random_source.next_index(reqs.size());
    reqs[idx] = mutate_requirement(reqs[idx], atoms, random_source);
    return Specification(std::set<Requirement>(reqs.begin(), reqs.end()),
                         specification.m_in_atoms, specification.m_out_atoms);
}
