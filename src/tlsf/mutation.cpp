#include "tlsf/mutation.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "genetic/mutation.hpp"
#include "prop_formula.hpp"

namespace {

using Section = std::vector<Formula>;

// The four unary operators Brizzio's mutation may introduce, o1 ∈ {¬, X, F, G}.
Formula::Kind pick_unary_kind(const RandomSource& random_source) {
    switch (random_source.next_index(4)) {
        case 0:
            return Formula::Kind::Not;
        case 1:
            return Formula::Kind::Next;
        case 2:
            return Formula::Kind::Eventually;
        case 3:
            return Formula::Kind::Globally;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

// Replacement binary operator for case (3), o2' ∈ {∨, ∧, U, R, W}. Implies/Iff
// are deliberately excluded (as in Brizzio's fragment): a mutated binary node
// re-emerges as one of these five, which is what lets the operator move a
// formula between purely propositional and genuinely temporal shapes.
Formula::Kind pick_binary_kind(const RandomSource& random_source) {
    switch (random_source.next_index(5)) {
        case 0:
            return Formula::Kind::And;
        case 1:
            return Formula::Kind::Or;
        case 2:
            return Formula::Kind::Until;
        case 3:
            return Formula::Kind::Release;
        case 4:
            return Formula::Kind::WeakUntil;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

// Connective used in case (2d), o2' ∈ {U, W, ∧, ∨}, to graft a fresh atom onto
// the mutated child.
Formula::Kind pick_connective_kind(const RandomSource& random_source) {
    switch (random_source.next_index(4)) {
        case 0:
            return Formula::Kind::Until;
        case 1:
            return Formula::Kind::WeakUntil;
        case 2:
            return Formula::Kind::And;
        case 3:
            return Formula::Kind::Or;
        default:
            assert(false);
            __builtin_unreachable();
    }
}

// Case (1a)/(1b): a boolean constant is flipped; any other atom is replaced by
// a pool atom, preferring a distinct one (Brizzio's q ≠ p) when the pool
// allows.
std::string flip_or_replace_atom(const std::string& atom,
                                 const std::vector<std::string>& atoms,
                                 const RandomSource& random_source) {
    if (atom == "true") {
        return "false";
    }
    if (atom == "false") {
        return "true";
    }
    assert(!atoms.empty());
    std::size_t index = random_source.next_index(atoms.size());
    if (atoms[index] == atom && atoms.size() > 1) {
        index = (index + 1) % atoms.size();
    }
    return atoms[index];
}

// A recursive re-implementation of the mutation operator from Brizzio et al.,
// "Automated Repair of Unrealisable LTL Specifications Guided by Model
// Counting". Unlike mutate_propositional_parts it may add, remove, or swap
// temporal operators, so it changes a formula's temporal skeleton. At each node
// one rewrite rule is drawn uniformly and applied, recursing into children; the
// three top-level cases mirror the paper's (1) atom/constant, (2) unary
// operator, (3) binary operator. @p atoms is the side-appropriate atom pool
// (inputs on the assumption side — or inputs ∪ outputs there too under
// allow_output_assumptions — and inputs ∪ outputs on the guarantee side),
// assumed non-empty.
Formula mutate_temporal(const Formula& formula,
                        const std::vector<std::string>& atoms,
                        const RandomSource& random_source) {
    switch (formula.kind()) {
        case Formula::Kind::Atom: {
            // Case (1): (a)/(b) replace the atom, or (c) wrap it in a unary op.
            const auto name = formula.atom_name();
            if (!name.has_value()) {
                assert(false);
                __builtin_unreachable();
            }
            if (random_source.next_index(3) == 0) {
                return Formula::make_unary(pick_unary_kind(random_source),
                                           formula);
            }
            return Formula::make_atom(
                flip_or_replace_atom(*name, atoms, random_source));
        }
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally: {
            // Case (2): φ = o1 φ1.
            const auto child = formula.unary_child();
            if (!child.has_value()) {
                return formula;
            }
            Formula mutated_child =
                mutate_temporal(*child, atoms, random_source);
            switch (random_source.next_index(4)) {
                case 0:  // (a) drop o1.
                    return mutated_child;
                case 1:  // (b) replace o1.
                    return Formula::make_unary(pick_unary_kind(random_source),
                                               mutated_child);
                case 2:  // (c) prepend a unary op, keeping o1.
                    return Formula::make_unary(
                        pick_unary_kind(random_source),
                        Formula::make_unary(formula.kind(), mutated_child));
                case 3: {  // (d) p o2' (o1' mutate(φ1)).
                    const Formula anchor = Formula::make_atom(
                        atoms[random_source.next_index(atoms.size())]);
                    const Formula inner = Formula::make_unary(
                        pick_unary_kind(random_source), mutated_child);
                    return Formula::make_binary(
                        pick_connective_kind(random_source), anchor, inner);
                }
                default:
                    assert(false);
                    __builtin_unreachable();
            }
        }
        default: {
            // Case (3): φ = φ1 o2 φ2 (any binary node, temporal or boolean).
            const auto children = formula.binary_children();
            if (!children.has_value()) {
                return formula;
            }
            switch (random_source.next_index(3)) {
                case 0: {  // (a) collapse to one mutated child.
                    const Formula& chosen = random_source.next_bool()
                                                ? children->first
                                                : children->second;
                    return mutate_temporal(chosen, atoms, random_source);
                }
                case 1:  // (b) mutate both children under a new binary op.
                    return Formula::make_binary(
                        pick_binary_kind(random_source),
                        mutate_temporal(children->first, atoms, random_source),
                        mutate_temporal(children->second, atoms,
                                        random_source));
                case 2:  // (c) as (b), then wrap in a unary op.
                    return Formula::make_unary(
                        pick_unary_kind(random_source),
                        Formula::make_binary(
                            pick_binary_kind(random_source),
                            mutate_temporal(children->first, atoms,
                                            random_source),
                            mutate_temporal(children->second, atoms,
                                            random_source)));
                default:
                    assert(false);
                    __builtin_unreachable();
            }
        }
    }
}

// The three section vectors on one side of the specification, paired so a
// chosen formula can be located and rewritten in place.
std::vector<Section*> side_sections(tlsf::Specification& spec,
                                    bool assumption_side) {
    if (assumption_side) {
        return {&spec.m_initially, &spec.m_require, &spec.m_assume};
    }
    return {&spec.m_preset, &spec.m_assert, &spec.m_guarantee};
}

std::size_t side_formula_count(const std::vector<Section*>& sections) {
    std::size_t total = 0;
    for (const Section* section : sections) {
        total += section->size();
    }
    return total;
}

// Mutates only the maximal propositional subtrees of @p formula, treating every
// temporal node as a fixed boundary that is reconstructed verbatim around its
// (recursively mutated) children. This preserves the temporal skeleton exactly:
// mutate_formula is only ever applied to a subtree with no temporal operators,
// so it can neither introduce nor drop an X/F/G/U/R/W node. Applying
// mutate_formula to the whole formula would not be safe — its propositional
// rewrites can discard a child, which would delete a nested temporal subtree.
Formula mutate_propositional_parts(const Formula& formula,
                                   const std::vector<std::string>& atoms,
                                   const RandomSource& random_source) {
    if (formula.is_propositional()) {
        return mutate_formula(formula, atoms, random_source);
    }
    switch (formula.kind()) {
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally: {
            const auto child = formula.unary_child();
            if (!child.has_value()) {
                return formula;
            }
            return Formula::make_unary(
                formula.kind(),
                mutate_propositional_parts(*child, atoms, random_source));
        }
        default: {
            const auto children = formula.binary_children();
            if (!children.has_value()) {
                return formula;
            }
            return Formula::make_binary(
                formula.kind(),
                mutate_propositional_parts(children->first, atoms,
                                           random_source),
                mutate_propositional_parts(children->second, atoms,
                                           random_source));
        }
    }
}

// Appends a new environment assumption to the ASSUME section. Strengthening the
// environment this way is how the algorithm repairs unrealizability the
// rewrite-only mutation cannot reach (e.g. the missing request-fairness of an
// unrealizable GR(1) arbiter). By default the assumption is an unconditional
// fairness property `G F <input>` (input negated on a coin flip), drawn from
// the inputs only. With allow_output_assumptions the atom pool widens to inputs
// ∪ outputs and a conditional form `G(c -> F r)` becomes reachable (guarded by
// p_conditional_assumption), so the search can express reactive-environment
// assumptions that reference outputs — e.g. `G(<output> -> F <input>)`, an
// environment obligation conditioned on a system output. The syntactic
// input-only ban is then replaced by the well-separation filter, which prunes
// any assumption the system could force to fail (such as the `G F <output>` the
// wider draw can also produce). With the flag off the draw is byte-for-byte
// identical to before.
tlsf::Specification tlsf_add_assumption(const tlsf::Specification& spec,
                                        const RandomSource& random_source,
                                        const Config& cfg) {
    tlsf::Specification mutated = spec;
    if (!cfg.allow_output_assumptions) {
        const std::string& signal =
            spec.m_inputs[random_source.next_index(spec.m_inputs.size())];
        Formula atom = Formula::make_atom(signal);
        if (random_source.next_bool()) {
            atom = Formula::make_unary(Formula::Kind::Not, atom);
        }
        mutated.m_assume.push_back(Formula::make_unary(
            Formula::Kind::Globally,
            Formula::make_unary(Formula::Kind::Eventually, atom)));
        return mutated;
    }
    std::vector<std::string> pool = spec.m_inputs;
    pool.insert(pool.end(), spec.m_outputs.begin(), spec.m_outputs.end());
    const auto draw = [&pool, &random_source]() {
        Formula atom =
            Formula::make_atom(pool[random_source.next_index(pool.size())]);
        if (random_source.next_bool()) {
            atom = Formula::make_unary(Formula::Kind::Not, atom);
        }
        return atom;
    };
    Formula body = Formula::make_unary(Formula::Kind::Eventually, draw());
    if (random_source.next_real() < cfg.p_conditional_assumption) {
        body = Formula::make_binary(Formula::Kind::Implies, draw(), body);
    }
    mutated.m_assume.push_back(
        Formula::make_unary(Formula::Kind::Globally, body));
    return mutated;
}

}  // namespace

tlsf::Specification tlsf_mutate(const tlsf::Specification& spec,
                                const RandomSource& random_source,
                                const Config& cfg) {
    // Low-probability structural action: add a new environment assumption.
    // Available whenever the assumption atom pool is non-empty: inputs, plus
    // outputs when allow_output_assumptions is set (so a spec with outputs but
    // no inputs can still gain an assumption).
    const bool have_assumption_pool =
        !spec.m_inputs.empty() ||
        (cfg.allow_output_assumptions && !spec.m_outputs.empty());
    if (have_assumption_pool &&
        random_source.next_real() < cfg.p_add_assumption) {
        return tlsf_add_assumption(spec, random_source, cfg);
    }
    tlsf::Specification mutated = spec;

    bool assumption_side = random_source.next_real() < cfg.tlsf_p_assumption;
    std::vector<Section*> sections = side_sections(mutated, assumption_side);
    if (side_formula_count(sections) == 0) {
        // Fall back to the other side when the chosen one is empty.
        assumption_side = !assumption_side;
        sections = side_sections(mutated, assumption_side);
    }
    const std::size_t total = side_formula_count(sections);
    if (total == 0) {
        return spec;
    }

    // Guarantee-side atom pool is inputs ∪ outputs; assumption-side is inputs,
    // widened to inputs ∪ outputs under allow_output_assumptions so a rewrite
    // can keep or introduce an output atom (letting an output-referencing
    // assumption from tlsf_add_assumption be reshaped — e.g. its F grown into a
    // W hold-until form — rather than having the output overwritten). The
    // syntactic ban is then replaced by the well-separation filter, which
    // prunes any assumption the system could force to fail.
    std::vector<std::string> pool = mutated.m_inputs;
    if (!assumption_side || cfg.allow_output_assumptions) {
        pool.insert(pool.end(), mutated.m_outputs.begin(),
                    mutated.m_outputs.end());
    }
    if (pool.empty()) {
        // Without atoms, mutate_formula's structural rewrites cannot draw a
        // replacement atom; leave the specification unchanged.
        return spec;
    }

    std::size_t choice = random_source.next_index(total);
    for (Section* section : sections) {
        if (choice < section->size()) {
            const bool temporal =
                random_source.next_real() < cfg.tlsf_p_temporal;
            (*section)[choice] =
                temporal
                    ? mutate_temporal((*section)[choice], pool, random_source)
                    : mutate_propositional_parts((*section)[choice], pool,
                                                 random_source);
            return mutated;
        }
        choice -= section->size();
    }
    return spec;
}
