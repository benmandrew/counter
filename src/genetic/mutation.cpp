#include "genetic/mutation.hpp"

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

Formula::Kind pick_binary_kind(const BooleanRandomSource& random_bool) {
    const bool high_bit = random_bool();
    const bool low_bit = random_bool();
    const int selector = (high_bit ? 2 : 0) + (low_bit ? 1 : 0);
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
                             const BooleanRandomSource& random_bool) {
    if (atom == "true") {
        return "false";
    }
    if (atom == "false") {
        return "true";
    }

    if (random_bool()) {
        return atom + "_mut";
    }

    if (atom.size() > 4 && atom.substr(atom.size() - 4) == "_mut") {
        return atom.substr(0, atom.size() - 4);
    }
    return "mut_" + atom;
}

Formula mutate_atom_formula(const Formula& formula,
                            const BooleanRandomSource& random_bool) {
    const std::optional<std::string> atom = formula.atom_name();
    if (!atom.has_value()) {
        throw std::logic_error("Expected atomic formula for atom mutation.");
    }

    if (random_bool()) {
        return Formula::make_atom(mutate_atom_name(*atom, random_bool));
    }

    return Formula::make_unary(Formula::Kind::Not, formula);
}

}  // namespace

Formula mutate_formula(const Formula& formula,
                       const BooleanRandomSource& boolean_random_source) {
    if (!boolean_random_source) {
        throw std::invalid_argument("boolean_random_source must be callable.");
    }
    const auto mutation_function =
        [&](const Formula& subtree) -> std::optional<Formula> {
        if (!boolean_random_source()) {
            return std::nullopt;
        }
        switch (subtree.kind()) {
            case Formula::Kind::Atom:
                return mutate_atom_formula(subtree, boolean_random_source);
            case Formula::Kind::Not: {
                const Formula child = subtree.unary_child().value();
                if (!boolean_random_source()) {
                    return child;
                }
                if (!boolean_random_source()) {
                    return Formula::make_unary(Formula::Kind::Not, child);
                }
                if (!boolean_random_source()) {
                    return Formula::make_unary(
                        Formula::Kind::Not,
                        Formula::make_unary(Formula::Kind::Not, child));
                }
                const Formula anchor = Formula::make_atom("p_mut");
                return Formula::make_binary(
                    pick_binary_kind(boolean_random_source), anchor,
                    Formula::make_unary(Formula::Kind::Not, child));
            }
            case Formula::Kind::And:
            case Formula::Kind::Or:
            case Formula::Kind::Implies:
            case Formula::Kind::Iff: {
                const auto children = subtree.binary_children().value();
                if (!boolean_random_source()) {
                    return boolean_random_source() ? children.first
                                                   : children.second;
                }
                const Formula combined = Formula::make_binary(
                    pick_binary_kind(boolean_random_source), children.first,
                    children.second);
                if (!boolean_random_source()) {
                    return combined;
                }
                return Formula::make_unary(Formula::Kind::Not, combined);
            }
        }
        return std::nullopt;
    };
    return formula.rewrite_post_order(mutation_function);
}

Requirement mutate_requirement(
    const Requirement& requirement,
    const BooleanRandomSource& boolean_random_source) {
    Requirement mutated = requirement;
    mutated.m_response =
        mutate_formula(requirement.m_response, boolean_random_source);
    mutated.m_trigger =
        mutate_formula(requirement.m_trigger, boolean_random_source);
    return mutated;
}

Requirement mutate_requirement(const Requirement& requirement) {
    return mutate_requirement(requirement,
                              []() { return (std::rand() % 2) == 0; });
}
