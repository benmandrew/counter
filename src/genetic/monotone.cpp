#include "genetic/monotone.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "genetic/mutation.hpp"
#include "genetic/random_source.hpp"
#include "prop_formula.hpp"

namespace {

// Whether replacing a subformula by a logically weaker one weakens the whole
// formula (Positive) or strengthens it (Negative). Not flips the polarity, so
// does the antecedent of an Implies, and a child of an Iff has neither: a
// biconditional is monotone in nothing, so nothing can be said about which way
// the whole formula moves.
enum class Polarity : std::uint8_t { Positive, Negative, Indeterminate };

Polarity flip(Polarity polarity) {
    switch (polarity) {
        case Polarity::Positive:
            return Polarity::Negative;
        case Polarity::Negative:
            return Polarity::Positive;
        default:
            return Polarity::Indeterminate;
    }
}

// The polarity each child of @p formula occurs under, given the parent's.
std::pair<Polarity, Polarity> child_polarities(const Formula& formula,
                                               Polarity polarity) {
    switch (formula.kind()) {
        case Formula::Kind::Not:
            return {flip(polarity), flip(polarity)};
        case Formula::Kind::Implies:
            return {flip(polarity), polarity};
        case Formula::Kind::Iff:
            return {Polarity::Indeterminate, Polarity::Indeterminate};
        default:
            // And, Or, X, F, G, U, R and W are all monotone increasing in
            // every argument, so a child keeps its parent's polarity.
            return {polarity, polarity};
    }
}

// One monotone rewrite. Named for what it does to the node it fires at, which
// is the weakening direction under Positive polarity and the strengthening one
// under Negative; the caller resolves that before choosing.
enum class MonotoneRule : std::uint8_t {
    // φ → true, or φ → false.
    Constant,
    // a ∧ b → a, or a ∨ b → a.
    DropOperand,
    // φ → φ ∨ ℓ when weakening, φ → φ ∧ ℓ when strengthening.
    AddOperand,
    // G φ → F φ.
    GloballyToEventually,
    // G φ → G F φ.
    GloballyToInfinitelyOften,
    // F φ → G φ.
    EventuallyToGlobally,
    // φ U ψ → φ W ψ.
    UntilToWeakUntil,
    // φ W ψ → φ U ψ.
    WeakUntilToUntil,
    // a ↔ b → a → b, or b → a.
    IffToImplies,
    // φ R ψ → ψ.
    ReleaseToConsequent,
    // φ R ψ → G ψ.
    ReleaseToGlobally,
    // X φ → F φ.
    NextToEventually,
    // X φ → G φ.
    NextToGlobally,
    // a ↔ b → a ∧ b, or ¬a ∧ ¬b.
    IffToConjunction,
};

// The rules cfg.tlsf_monotone_extra_rules adds at @p formula's kind, empty at
// every other kind.
//
// Release carried no monotone rule while its duals Until and WeakUntil each
// carry one, Next carried none, and Iff carried the weakening to one of its
// implications and nothing the other way — so at all three the whole monotone
// move was the rewrite to a constant, which guts the node. Each of the five
// below is a disjunct or an operand of the semantics it fires at:
// `φ R ψ ≡ G ψ | (ψ U (ψ ∧ φ))` gives both Release rules,
// `F φ ≡ φ | X φ | XX φ | …` and `G φ ≡ φ ∧ X G φ` give both Next rules, and
// `a ∧ b` and `¬a ∧ ¬b` each entail `a ↔ b`.
std::vector<MonotoneRule> extra_rules_at(const Formula& formula, bool weaken) {
    switch (formula.kind()) {
        case Formula::Kind::Release:
            return {weaken ? MonotoneRule::ReleaseToConsequent
                           : MonotoneRule::ReleaseToGlobally};
        case Formula::Kind::Next:
            return {weaken ? MonotoneRule::NextToEventually
                           : MonotoneRule::NextToGlobally};
        case Formula::Kind::Iff:
            if (weaken) {
                return {};
            }
            return {MonotoneRule::IffToConjunction};
        default:
            return {};
    }
}

// The rules that fire at @p formula's own kind. Constant is always one of
// them, so the list is never empty and every node is a site.
//
// Constant and AddOperand are the two rules sound at every node: `φ → true`
// and `φ → φ ∨ ℓ` weaken anything, `φ → false` and `φ → φ ∧ ℓ` strengthen
// anything. AddOperand was nonetheless offered at And and Or alone, which
// leaves an Atom with Constant as its only move — so the one rewrite that
// grows a literal into a disjunction is reachable only where a disjunction
// already stands, and gutting the node to `true` is the whole monotone menu
// at a literal. Every assumption-shaped ideal in the corpus is a disjunction
// built out of literals, and AuRUS reaches them because its FormulaWeakening
// applies `a → a | b` at a literal.
//
// @p rules.atom_rules (cfg.tlsf_monotone_atom_rules, default false) makes the
// wider menu opt-in. Off, this returns the menu the binary held before the key
// existed — the same rules in the same order, so next_index(rules.size()) draws
// the same value and every draw after it follows. That reproduction is the
// whole reason the gate exists, so the two branches below must stay written out
// rather than folded into one push_back with a condition on the rule.
//
// @p rules.extra_rules (cfg.tlsf_monotone_extra_rules, default false) appends
// extra_rules_at's result to all of that, which leaves the list built here
// unchanged in content and order when the key is off.
std::vector<MonotoneRule> rules_at(const Formula& formula, bool weaken,
                                   MonotoneRules rules_offered) {
    const bool atom_rules = rules_offered.atom_rules;
    std::vector<MonotoneRule> rules = {MonotoneRule::Constant};
    if (atom_rules) {
        rules.push_back(MonotoneRule::AddOperand);
    }
    switch (formula.kind()) {
        case Formula::Kind::And:
            if (weaken) {
                rules.push_back(MonotoneRule::DropOperand);
            } else if (!atom_rules) {
                rules.push_back(MonotoneRule::AddOperand);
            }
            break;
        case Formula::Kind::Or:
            if (!weaken) {
                rules.push_back(MonotoneRule::DropOperand);
            } else if (!atom_rules) {
                rules.push_back(MonotoneRule::AddOperand);
            }
            break;
        case Formula::Kind::Globally:
            if (weaken) {
                rules.push_back(MonotoneRule::GloballyToEventually);
                rules.push_back(MonotoneRule::GloballyToInfinitelyOften);
            }
            break;
        case Formula::Kind::Eventually:
            if (!weaken) {
                rules.push_back(MonotoneRule::EventuallyToGlobally);
            }
            break;
        case Formula::Kind::Until:
            if (weaken) {
                rules.push_back(MonotoneRule::UntilToWeakUntil);
            }
            break;
        case Formula::Kind::WeakUntil:
            if (!weaken) {
                rules.push_back(MonotoneRule::WeakUntilToUntil);
            }
            break;
        case Formula::Kind::Iff:
            if (weaken) {
                rules.push_back(MonotoneRule::IffToImplies);
            }
            break;
        default:
            break;
    }
    if (rules_offered.extra_rules) {
        const std::vector<MonotoneRule> extra = extra_rules_at(formula, weaken);
        rules.insert(rules.end(), extra.begin(), extra.end());
    }
    return rules;
}

// The operands of a node a rule fires at. Every rule is filed under the kind
// it applies to, so the shape is a precondition rather than a case to handle:
// there is no operand to return if the node has none, and rules_at is what
// keeps that from arising. The assert states it, and the suppression is on the
// access alone rather than on the file, since a second unchecked optional in
// either function should still be reported. test_monotone_rewrite_direction-
// _holds is what would catch a rule filed under the wrong kind.
Formula unary_operand(const Formula& formula) {
    const auto child = formula.unary_child();
    assert(child.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): precondition above.
    return *child;
}

std::pair<Formula, Formula> binary_operands(const Formula& formula) {
    const auto children = formula.binary_children();
    assert(children.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): precondition above.
    return *children;
}

Formula apply_monotone_rule(const Formula& formula, MonotoneRule rule,
                            bool weaken, const std::vector<std::string>& atoms,
                            const RandomSource& random_source) {
    switch (rule) {
        case MonotoneRule::Constant:
            return Formula::make_atom(weaken ? "true" : "false");
        case MonotoneRule::DropOperand: {
            const std::pair<Formula, Formula> operands =
                binary_operands(formula);
            return random_source.next_bool() ? operands.first : operands.second;
        }
        case MonotoneRule::AddOperand: {
            const Formula literal = draw_literal(atoms, random_source);
            // The connective comes from the direction rather than the
            // node's own kind. This is deliberately ungated: where the legacy
            // menu offers AddOperand at all — an And node being strengthened,
            // an Or node being weakened — `weaken ? Or : And` is the node's
            // own kind, so with tlsf_monotone_atom_rules off the two spellings
            // agree on every formula. Only rules_at needs the gate.
            return Formula::make_binary(
                weaken ? Formula::Kind::Or : Formula::Kind::And, formula,
                literal);
        }
        case MonotoneRule::GloballyToEventually:
            return Formula::make_unary(Formula::Kind::Eventually,
                                       unary_operand(formula));
        case MonotoneRule::GloballyToInfinitelyOften:
            return Formula::make_unary(Formula::Kind::Globally, formula);
        case MonotoneRule::EventuallyToGlobally:
            return Formula::make_unary(Formula::Kind::Globally,
                                       unary_operand(formula));
        case MonotoneRule::UntilToWeakUntil: {
            const std::pair<Formula, Formula> operands =
                binary_operands(formula);
            return Formula::make_binary(Formula::Kind::WeakUntil,
                                        operands.first, operands.second);
        }
        case MonotoneRule::WeakUntilToUntil: {
            const std::pair<Formula, Formula> operands =
                binary_operands(formula);
            return Formula::make_binary(Formula::Kind::Until, operands.first,
                                        operands.second);
        }
        case MonotoneRule::IffToImplies: {
            const std::pair<Formula, Formula> operands =
                binary_operands(formula);
            // Both directions of a biconditional are weakenings of it, so
            // which one is a further draw rather than a fixed choice.
            return random_source.next_bool()
                       ? Formula::make_binary(Formula::Kind::Implies,
                                              operands.first, operands.second)
                       : Formula::make_binary(Formula::Kind::Implies,
                                              operands.second, operands.first);
        }
        case MonotoneRule::ReleaseToConsequent:
            return binary_operands(formula).second;
        case MonotoneRule::ReleaseToGlobally:
            return Formula::make_unary(Formula::Kind::Globally,
                                       binary_operands(formula).second);
        case MonotoneRule::NextToEventually:
            return Formula::make_unary(Formula::Kind::Eventually,
                                       unary_operand(formula));
        case MonotoneRule::NextToGlobally:
            return Formula::make_unary(Formula::Kind::Globally,
                                       unary_operand(formula));
        case MonotoneRule::IffToConjunction: {
            const std::pair<Formula, Formula> operands =
                binary_operands(formula);
            // Both polarities of the agreement entail the biconditional, so
            // which one is a further draw, as IffToImplies draws between its
            // two directions.
            if (random_source.next_bool()) {
                return Formula::make_binary(Formula::Kind::And, operands.first,
                                            operands.second);
            }
            return Formula::make_binary(
                Formula::Kind::And,
                Formula::make_unary(Formula::Kind::Not, operands.first),
                Formula::make_unary(Formula::Kind::Not, operands.second));
        }
        default:
            assert(false);
            __builtin_unreachable();
    }
}

// Numbers every node of @p formula in pre-order and records the polarity each
// occurs under. Both walks here number in the same order, which is what lets a
// site be drawn in one pass and rewritten in the next.
void collect_polarities(const Formula& formula, Polarity polarity,
                        std::vector<Polarity>& out) {
    out.push_back(polarity);
    if (const auto child = formula.unary_child(); child.has_value()) {
        const auto polarities = child_polarities(formula, polarity);
        collect_polarities(*child, polarities.first, out);
        return;
    }
    if (const auto children = formula.binary_children(); children.has_value()) {
        const auto polarities = child_polarities(formula, polarity);
        collect_polarities(children->first, polarities.first, out);
        collect_polarities(children->second, polarities.second, out);
    }
}

// Rebuilds @p formula with one rule applied at the node numbered @p target.
// The walk stops descending there, so every node numbered afterwards takes a
// strictly larger index and no second node can match.
Formula rewrite_at_site(const Formula& formula, Polarity polarity,
                        std::size_t target, std::size_t& next, bool want_weaker,
                        MonotoneRules rules_offered,
                        const std::vector<std::string>& atoms,
                        const RandomSource& random_source) {
    const std::size_t index = next++;
    if (index == target) {
        // A weaker subformula weakens the whole only where it occurs
        // positively; under a negation the dual rule is what moves the whole
        // formula the way the caller asked for.
        const bool weaken_here =
            (polarity == Polarity::Positive) == want_weaker;
        const std::vector<MonotoneRule> rules =
            rules_at(formula, weaken_here, rules_offered);
        const std::size_t choice = random_source.next_index(rules.size());
        return apply_monotone_rule(formula, rules[choice], weaken_here, atoms,
                                   random_source);
    }
    if (const auto child = formula.unary_child(); child.has_value()) {
        const auto polarities = child_polarities(formula, polarity);
        return Formula::make_unary(
            formula.kind(),
            rewrite_at_site(*child, polarities.first, target, next, want_weaker,
                            rules_offered, atoms, random_source));
    }
    if (const auto children = formula.binary_children(); children.has_value()) {
        const auto polarities = child_polarities(formula, polarity);
        // Sequenced into locals: the target's branch draws, and argument
        // evaluation order is unspecified.
        const Formula left =
            rewrite_at_site(children->first, polarities.first, target, next,
                            want_weaker, rules_offered, atoms, random_source);
        const Formula right =
            rewrite_at_site(children->second, polarities.second, target, next,
                            want_weaker, rules_offered, atoms, random_source);
        return Formula::make_binary(formula.kind(), left, right);
    }
    return formula;
}

}  // namespace

Formula monotone_rewrite(const Formula& formula, MonotoneDirection direction,
                         MonotoneRules rules,
                         const std::vector<std::string>& atoms,
                         const RandomSource& random_source) {
    assert(!atoms.empty());
    std::vector<Polarity> polarities;
    collect_polarities(formula, Polarity::Positive, polarities);
    std::vector<std::size_t> sites;
    for (std::size_t index = 0; index < polarities.size(); ++index) {
        if (polarities[index] != Polarity::Indeterminate) {
            sites.push_back(index);
        }
    }
    if (sites.empty()) {
        // Only reachable when the root itself is indeterminate, which it never
        // is; a formula whose every node sits under a biconditional has no
        // monotone move and is returned as it stands.
        return formula;
    }
    const std::size_t site = random_source.next_index(sites.size());
    std::size_t next = 0;
    return rewrite_at_site(formula, Polarity::Positive, sites[site], next,
                           direction == MonotoneDirection::Weaken, rules, atoms,
                           random_source);
}
