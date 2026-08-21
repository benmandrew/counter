#include "tlsf/mutation.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "genetic/mutation.hpp"
#include "prop_formula.hpp"

namespace {

using tlsf::Section;

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

// Replacement binary operator for case (3), o2' ∈ {∨, ∧, →, U, R, W}. Brizzio's
// fragment is Owl's negation normal form, where `a -> b` is stored as a
// disjunction and there is no implication to re-emit; counter keeps Implies as
// a first-class node, so excluding it here made a biconditional reachable only
// to be destroyed. Three subjects are the whole ideal for that one
// substitution: ltl2dba-r-2's sole ideal is its input with the root `<->`
// replaced by `->`, and ltl2dba-theta-2 and ltl2dba27 are the same shape. The
// FRETISH twins in genetic/mutation.cpp and genetic/crossover.cpp already draw
// Implies, so the gap was TLSF-only.
//
// Iff stays out. The defect is that a biconditional could not be weakened, not
// that one could not be built, and the paper's fragment has no biconditional
// for the same reason it has no implication.
Formula::Kind pick_binary_kind(const RandomSource& random_source) {
    switch (random_source.next_index(6)) {
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
        case 5:
            return Formula::Kind::Implies;
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
                        const RandomSource& random_source, const Config& cfg) {
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
                mutate_temporal(*child, atoms, random_source, cfg);
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
            // Case (3): φ = φ1 o2 φ2 (any binary node, temporal or boolean),
            // Iff included, which is what makes `<->` → `->` reachable now
            // that pick_binary_kind draws Implies.
            //
            // Case (d) is counter's own, not Brizzio's. It preserves the
            // node's own connective, and without it an implication was
            // reachable only to be destroyed: a guarded implication — the
            // shape of every minimal guarantee weakening — survived no arm.
            // It is also this path's only structure-preserving move; every
            // other arm regenerates the conjunct.
            const auto children = formula.binary_children();
            if (!children.has_value()) {
                return formula;
            }
            switch (random_source.next_index(4)) {
                case 3: {  // (d) keep o2, mutating both children.
                    const Formula left = mutate_temporal(children->first, atoms,
                                                         random_source, cfg);
                    const Formula right = mutate_temporal(
                        children->second, atoms, random_source, cfg);
                    return Formula::make_binary(formula.kind(), left, right);
                }
                case 0: {  // (a) collapse to one mutated child.
                    const Formula& chosen = random_source.next_bool()
                                                ? children->first
                                                : children->second;
                    return mutate_temporal(chosen, atoms, random_source, cfg);
                }
                // The kind draw and both child mutations draw, and arguments of
                // one call are evaluated in an unspecified order, so each draw
                // is sequenced into a local to keep a seed reproducible across
                // compilers.
                case 1: {  // (b) mutate both children under a new binary op.
                    const Formula::Kind kind = pick_binary_kind(random_source);
                    const Formula left = mutate_temporal(children->first, atoms,
                                                         random_source, cfg);
                    const Formula right = mutate_temporal(
                        children->second, atoms, random_source, cfg);
                    return Formula::make_binary(kind, left, right);
                }
                case 2: {  // (c) as (b), then wrap in a unary op.
                    const Formula::Kind outer_kind =
                        pick_unary_kind(random_source);
                    const Formula::Kind inner_kind =
                        pick_binary_kind(random_source);
                    const Formula left = mutate_temporal(children->first, atoms,
                                                         random_source, cfg);
                    const Formula right = mutate_temporal(
                        children->second, atoms, random_source, cfg);
                    return Formula::make_unary(
                        outer_kind,
                        Formula::make_binary(inner_kind, left, right));
                }
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
        const auto sections = tlsf::mutable_assumption_sections_of(spec);
        return {sections[0], sections[1], sections[2]};
    }
    const auto sections = tlsf::mutable_guarantee_sections_of(spec);
    return {sections[0], sections[1], sections[2]};
}

// A conjunct a mutation may rewrite: its section, its slot in it, and which of
// the side's three sections it came from. Deleted conjuncts are left out, so a
// mutation is never spent rewriting content nothing reads.
//
// The section index is what makes a rewrite section-aware. Index 0 is the
// initial-condition section — INITIALLY on the assumption side, PRESET on the
// guarantee one — and basic TLSF requires both to be propositional, over the
// inputs and the outputs respectively. One pool and one temporal/propositional
// draw for the whole side ignored that, and the 2026-08-14-aurus-h2h corpus
// shows the result: 15 INITIALLY entries carrying a temporal operator and 8
// PRESET entries carrying an input, against none in any of the 25 inputs.
// Those repairs are outside the format they are written in.
struct Slot {
    Section* m_section;
    std::size_t m_index;
    std::size_t m_section_index;
};

std::vector<Slot> side_live_slots(const std::vector<Section*>& sections) {
    std::vector<Slot> slots;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        Section* section = sections[index];
        for (std::size_t i = 0; i < section->size(); ++i) {
            if (!(*section)[i].m_removed) {
                slots.push_back({section, i, index});
            }
        }
    }
    return slots;
}

// The initial-condition section admits neither a temporal operator nor a
// signal from the other side.
bool is_initial_condition_section(std::size_t section_index) {
    return section_index == 0;
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
                                   const RandomSource& random_source,
                                   const Config& cfg) {
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
                mutate_propositional_parts(*child, atoms, random_source, cfg));
        }
        default: {
            const auto children = formula.binary_children();
            if (!children.has_value()) {
                return formula;
            }
            // Sequenced into locals: both calls draw, and argument evaluation
            // order is unspecified.
            const Formula left = mutate_propositional_parts(
                children->first, atoms, random_source, cfg);
            const Formula right = mutate_propositional_parts(
                children->second, atoms, random_source, cfg);
            return Formula::make_binary(formula.kind(), left, right);
        }
    }
}

// Draws a literal from @p pool: an atom, negated on a coin flip.
Formula draw_literal(const std::vector<std::string>& pool,
                     const RandomSource& random_source) {
    const Formula atom =
        Formula::make_atom(pool[random_source.next_index(pool.size())]);
    return random_source.next_bool()
               ? Formula::make_unary(Formula::Kind::Not, atom)
               : atom;
}

// Modality of a conditional assumption's consequent, o ∈ {F, X, nothing}.
// `F` was hard-wired here until 2026-08-19, which put two whole families of
// ideal beyond the operator: rg1's `G(!valid -> X !cancel)` and minepump's
// `G(high_water -> !methane)`. Neither was reachable afterwards either, since
// mutate_propositional_parts reconstructs every temporal node verbatim and
// mutate_temporal destroys the implication it would have to keep. Of the
// 2026-08-14-aurus-h2h corpus's 783 appended assumptions, 742 still carried
// the template's F and one reached an X.
Formula apply_consequent_modality(const Formula& body,
                                  const RandomSource& random_source) {
    switch (random_source.next_index(3)) {
        case 0:
            return Formula::make_unary(Formula::Kind::Eventually, body);
        case 1:
            return Formula::make_unary(Formula::Kind::Next, body);
        default:
            return body;
    }
}

// Appends a new environment assumption to the ASSUME section. Strengthening the
// environment this way is how the algorithm repairs unrealizability the
// rewrite-only mutation cannot reach (e.g. the missing request-fairness of an
// unrealizable GR(1) arbiter).
//
// The unconditional form is a fairness property `G F <input>`; under
// p_conditional_assumption a guarded form `G(<guard> -> o <input>)` is drawn
// instead, o coming from apply_consequent_modality.
//
// The obliged literal is always an *input*, whatever allow_output_assumptions
// says. An assumption that obliges an output is one the system can defeat by
// withholding its own signal, which discharges every guarantee at a stroke.
// Well-separation was documented as the safeguard against that, and it only
// half is: it catches the unconditional `G F <output>`, of which the
// 2026-08-14-aurus-h2h corpus contains none, and it passes the guarded
// `G(<lit> -> F <output>)`, of which that corpus contains 180 across 175
// repairs. The check asks whether the environment *can* satisfy the
// assumptions, and an environment that never raises the guard can. Drawing the
// consequent from the inputs closes the gap without a new solver query.
//
// allow_output_assumptions here governs the guard alone, which is the reactive
// shape it exists for: conditioning on system behaviour adds no obligation the
// system can dodge. `G(<output> -> F <input>)` stays reachable, and
// `G(<input> -> F <output>)` does not.
tlsf::Specification tlsf_add_assumption(const tlsf::Specification& spec,
                                        const RandomSource& random_source,
                                        const Config& cfg) {
    if (spec.m_inputs.empty()) {
        // With no input there is nothing the environment alone can be obliged
        // to do, and every assumption expressible here would be one the system
        // could defeat.
        return spec;
    }
    tlsf::Specification mutated = spec;
    const Formula body = draw_literal(spec.m_inputs, random_source);
    if (random_source.next_real() >= cfg.p_conditional_assumption) {
        mutated.m_assume.emplace_back(Formula::make_unary(
            Formula::Kind::Globally,
            Formula::make_unary(Formula::Kind::Eventually, body)));
        return mutated;
    }
    std::vector<std::string> guard_pool = spec.m_inputs;
    if (cfg.allow_output_assumptions) {
        guard_pool.insert(guard_pool.end(), spec.m_outputs.begin(),
                          spec.m_outputs.end());
    }
    Formula guard = draw_literal(guard_pool, random_source);
    const Formula consequent = apply_consequent_modality(body, random_source);
    // `G(l -> F l)` and `G(l -> l)` are tautologies, and 30 of the corpus's
    // appended assumptions were one, the guard and the body being drawn
    // independently from overlapping pools. Flipping the guard's polarity
    // costs no draw and yields an assumption that says something. Under X no
    // flip is needed: `G(l -> X l)` is a persistence property, not a tautology.
    if (guard == consequent) {
        guard = Formula::make_unary(Formula::Kind::Not, guard);
        guard.remove_double_negation();
    }
    mutated.m_assume.emplace_back(Formula::make_unary(
        Formula::Kind::Globally,
        Formula::make_binary(Formula::Kind::Implies, guard, consequent)));
    return mutated;
}

// Deletes one guarantee-side conjunct (PRESET, ASSERT or GUARANTEE) by
// tombstoning it in place. The slot stays so that crossover still sees a
// matching shape and the similarity objectives keep pairing the same conjuncts;
// erasing it would shift the rest and start comparing unrelated formulae
// against the original.
//
// The mirror of tlsf_add_assumption: that one strengthens the environment, this
// one drops a system obligation. Some repairs are reachable no other way —
// every `drop-*` ideal in examples/ deletes an ASSERT conjunct, and for five
// TLSF subjects that is the only ideal there is.
tlsf::Specification tlsf_remove_guarantee(const tlsf::Specification& spec,
                                          const RandomSource& random_source) {
    tlsf::Specification mutated = spec;
    const auto sections = tlsf::mutable_guarantee_sections_of(mutated);
    const std::vector<Slot> slots =
        side_live_slots({sections[0], sections[1], sections[2]});
    assert(!slots.empty());
    const Slot& slot = slots[random_source.next_index(slots.size())];
    (*slot.m_section)[slot.m_index].m_removed = true;
    return mutated;
}

// The atom pool the grammar before 2026-08-19 drew from: the guarantee side
// takes inputs ∪ outputs, the assumption side the inputs, widened to
// inputs ∪ outputs under allow_output_assumptions so a rewrite can keep or
// introduce an output atom (letting a guard drawn by tlsf_add_assumption be
// reshaped rather than overwritten).
std::vector<std::string> side_atom_pool(const tlsf::Specification& spec,
                                        bool assumption_side,
                                        const Config& cfg) {
    std::vector<std::string> pool = spec.m_inputs;
    if (!assumption_side || cfg.allow_output_assumptions) {
        pool.insert(pool.end(), spec.m_outputs.begin(), spec.m_outputs.end());
    }
    return pool;
}

// The atom pool follows the section, not just the side. An initial condition
// is over one side's own signals alone: INITIALLY over the inputs, PRESET over
// the outputs. Every other section keeps the side pool above.
std::vector<std::string> section_atom_pool(const tlsf::Specification& spec,
                                           bool assumption_side,
                                           std::size_t section_index,
                                           const Config& cfg) {
    if (is_initial_condition_section(section_index)) {
        return assumption_side ? spec.m_inputs : spec.m_outputs;
    }
    return side_atom_pool(spec, assumption_side, cfg);
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
    // The mirror action: delete a guarantee-side conjunct. Never the last live
    // one, since a specification with nothing left to guarantee is realizable
    // by doing nothing and is no repair. The probability is read before the
    // draw so that a zero costs no draw at all, which is what lets a config set
    // it to 0 and reproduce a run from before the operator existed.
    if (cfg.p_remove_guarantee > 0.0 && tlsf::count_live_guarantees(spec) > 1 &&
        random_source.next_real() < cfg.p_remove_guarantee) {
        return tlsf_remove_guarantee(spec, random_source);
    }
    tlsf::Specification mutated = spec;

    // The draw is unconditional and sequenced into its own local so the number
    // of RandomSource draws does not depend on the config -- the determinism
    // goldens pin that stream.
    const double side_draw = random_source.next_real();
    bool assumption_side = side_draw < cfg.tlsf_p_assumption;
    std::vector<Section*> sections = side_sections(mutated, assumption_side);
    std::vector<Slot> slots = side_live_slots(sections);
    if (slots.empty()) {
        // Fall back to the other side when the chosen one has nothing left to
        // rewrite, whether because it is absent or because every conjunct on it
        // has been deleted.
        assumption_side = !assumption_side;
        sections = side_sections(mutated, assumption_side);
        slots = side_live_slots(sections);
    }
    if (slots.empty()) {
        return spec;
    }

    // The atom pool follows the *section*, so the slot is drawn before the
    // pool can be built.
    const std::size_t slot_index = random_source.next_index(slots.size());
    const std::vector<std::string> pool = section_atom_pool(
        mutated, assumption_side, slots[slot_index].m_section_index, cfg);
    if (pool.empty()) {
        // Without atoms, mutate_formula's structural rewrites cannot draw a
        // replacement atom; leave the specification unchanged.
        return spec;
    }
    const Slot& slot = slots[slot_index];

    tlsf::SectionEntry& entry = (*slot.m_section)[slot.m_index];
    // An initial condition must stay propositional, so it takes the rewrite
    // that preserves the temporal skeleton — of which it has none — rather
    // than the one that introduces operators. The short circuit is deliberate:
    // an initial condition costs no temporal draw at all.
    const bool initial_condition =
        is_initial_condition_section(slot.m_section_index);
    const bool temporal =
        !initial_condition && random_source.next_real() < cfg.tlsf_p_temporal;
    entry.m_formula =
        temporal ? mutate_temporal(entry.m_formula, pool, random_source, cfg)
                 : mutate_propositional_parts(entry.m_formula, pool,
                                              random_source, cfg);
    return mutated;
}
