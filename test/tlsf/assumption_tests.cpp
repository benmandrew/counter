// Tests over the 2026-08-25 assumption-reach operators: the compositional body
// grammar and the bare-F form in tlsf_add_assumption, tlsf_remove_assumption,
// and the mutation burst.
//
// Each operator has a value at which it is a no-op, and every one of them is
// asserted to cost no RandomSource draw there. That is what lets an archived
// campaign reproduce against a binary that has these keys, and it is not free:
// a probability read after the draw rather than before shifts the stream even
// when the operator never fires.
#include <cstddef>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/mutation.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

tlsf::Specification three_input_spec() {
    return tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { b1; b2; b3; } "
        "OUTPUTS { f1; }\nGUARANTEE { G (b1 -> F f1); }\n}\n");
}

// Counts draws so a "costs no draw" claim is measured rather than assumed.
std::size_t draws_for(const tlsf::Specification& spec, const Config& cfg,
                      std::size_t seeds) {
    std::size_t total = 0;
    for (std::size_t seed = 0; seed < seeds; ++seed) {
        const auto counter = std::make_shared<std::size_t>(0);
        std::mt19937 engine(static_cast<std::uint32_t>(seed));
        RandomSource source([engine, counter](std::size_t bound) mutable {
            ++*counter;
            return static_cast<std::size_t>(
                bounded_uniform(engine, static_cast<std::uint32_t>(bound)));
        });
        (void)tlsf_mutate(spec, source, cfg);
        total += *counter;
    }
    return total;
}

bool mentions(const Formula& formula, Formula::Kind kind) {
    if (formula.kind() == kind) {
        return true;
    }
    if (const auto child = formula.unary_child()) {
        return mentions(*child, kind);
    }
    if (const auto pair = formula.binary_children()) {
        return mentions(pair->first, kind) || mentions(pair->second, kind);
    }
    return false;
}

void collect_atoms(const Formula& formula, std::vector<std::string>& into) {
    if (const auto name = formula.atom_name()) {
        into.push_back(*name);
        return;
    }
    if (const auto child = formula.unary_child()) {
        collect_atoms(*child, into);
        return;
    }
    if (const auto pair = formula.binary_children()) {
        collect_atoms(pair->first, into);
        collect_atoms(pair->second, into);
    }
}

// -- the body grammar ---------------------------------------------------------

// At width 1 the body is a single literal, which is what the operator emitted
// before the grammar existed.
void test_width_one_draws_a_single_literal() {
    const tlsf::Specification spec = three_input_spec();
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.tlsf_max_assumption_width = 1;
    cfg.p_conditional_assumption = 0.0;  // the unconditional G F form
    for (std::size_t seed = 0; seed < 40; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        expect(mutated.m_assume.size() == 1,
               "assumption: width 1 appends exactly one assumption");
        const Formula& body = mutated.m_assume.front().m_formula;
        expect(!mentions(body, Formula::Kind::Or) &&
                   !mentions(body, Formula::Kind::And),
               "assumption: width 1 draws a single literal, no connective");
    }
}

// Above width 1 the grammar must actually reach both connectives, or it is the
// old template with extra draws. lift's ideal is a disjunction and
// humanoid-503's is a conjunction, so neither alone would do.
void test_wide_bodies_reach_both_connectives() {
    const tlsf::Specification spec = three_input_spec();
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.tlsf_max_assumption_width = 3;
    cfg.p_conditional_assumption = 0.0;
    bool saw_or = false;
    bool saw_and = false;
    for (std::size_t seed = 0; seed < 200 && !(saw_or && saw_and); ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        const Formula& body = mutated.m_assume.front().m_formula;
        saw_or = saw_or || mentions(body, Formula::Kind::Or);
        saw_and = saw_and || mentions(body, Formula::Kind::And);
    }
    expect(saw_or, "assumption: a wide body reaches a disjunction");
    expect(saw_and, "assumption: a wide body reaches a conjunction");
}

// Every atom of an appended assumption is an input, whatever the width: an
// assumption obliging an output is one the system defeats by withholding its
// own signal.
void test_wide_bodies_use_inputs_only() {
    const tlsf::Specification spec = three_input_spec();
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.tlsf_max_assumption_width = 3;
    cfg.allow_output_assumptions = false;
    for (std::size_t seed = 0; seed < 60; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        const Formula& body = mutated.m_assume.front().m_formula;
        std::vector<std::string> atoms;
        collect_atoms(body, atoms);
        for (const std::string& atom : atoms) {
            expect(atom != "f1",
                   "assumption: a wide body never obliges an output");
        }
    }
}

// -- the bare form ------------------------------------------------------------

// lily11's whole ideal is `F req`. Wrapped in G it is strictly stronger, so the
// bare form has to be drawn rather than rewritten into.
void test_bare_assumption_is_reachable() {
    const tlsf::Specification spec = three_input_spec();
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.p_conditional_assumption = 0.0;
    cfg.tlsf_p_bare_assumption = 1.0;
    for (std::size_t seed = 0; seed < 20; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        const Formula& body = mutated.m_assume.front().m_formula;
        expect(body.kind() == Formula::Kind::Eventually,
               "assumption: p_bare = 1 emits F body, not G F body");
    }
    Config wrapped = cfg;
    wrapped.tlsf_p_bare_assumption = 0.0;
    for (std::size_t seed = 0; seed < 20; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, wrapped);
        const Formula& body = mutated.m_assume.front().m_formula;
        expect(body.kind() == Formula::Kind::Globally,
               "assumption: p_bare = 0 keeps the G F form");
    }
}

// -- removing an assumption ---------------------------------------------------

// Tombstoned in place, never erased: every comparison pairs specifications by
// position, so a shifted section scores slot i against the original's slot i+1.
void test_remove_assumption_tombstones_in_place() {
    tlsf::Specification spec = tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { b1; } OUTPUTS { f1; }\n"
        "ASSUME { G (F b1); G (b1 -> b1); }\n"
        "GUARANTEE { G (b1 -> F f1); }\n}\n");
    const std::size_t before = spec.m_assume.size();
    Config cfg;
    cfg.p_add_assumption = 0.0;
    cfg.p_remove_guarantee = 0.0;
    cfg.tlsf_p_remove_assumption = 1.0;
    bool removed_one = false;
    for (std::size_t seed = 0; seed < 20; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        expect(mutated.m_assume.size() == before,
               "remove-assumption: the section keeps its length");
        std::size_t live = 0;
        for (const tlsf::SectionEntry& entry : mutated.m_assume) {
            live += entry.m_removed ? 0 : 1;
        }
        removed_one = removed_one || live == before - 1;
        expect(live >= before - 1,
               "remove-assumption: at most one conjunct goes per mutation");
    }
    expect(removed_one, "remove-assumption: a live conjunct is tombstoned");
}

// Unlike the guarantee side there is no floor of one: a specification that
// assumes nothing of its environment is meaningful.
void test_remove_assumption_has_no_floor() {
    tlsf::Specification spec = tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { b1; } OUTPUTS { f1; }\n"
        "ASSUME { G (F b1); }\nGUARANTEE { G (b1 -> F f1); }\n}\n");
    Config cfg;
    cfg.p_add_assumption = 0.0;
    cfg.p_remove_guarantee = 0.0;
    cfg.tlsf_p_remove_assumption = 1.0;
    const RandomSource rng = make_random_source_from_seed(0);
    const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
    expect(mutated.m_assume.front().m_removed,
           "remove-assumption: the last assumption may go");
}

// -- the burst ----------------------------------------------------------------

// At 0 every mutation is single, which is the contract every test written
// before the burst existed assumes.
void test_burst_zero_applies_one_mutation() {
    const tlsf::Specification spec = three_input_spec();
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.tlsf_p_burst_continue = 0.0;
    for (std::size_t seed = 0; seed < 40; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        expect(tlsf_mutate(spec, rng, cfg).m_assume.size() == 1,
               "burst: at 0 exactly one mutation is applied");
    }
}

// Above 0 a burst must actually reach more than one edit, and must respect the
// cap of 8 rather than running away.
void test_burst_reaches_several_and_stops() {
    const tlsf::Specification spec = three_input_spec();
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.tlsf_p_burst_continue = 0.9;
    std::size_t widest = 0;
    for (std::size_t seed = 0; seed < 60; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const std::size_t appended =
            tlsf_mutate(spec, rng, cfg).m_assume.size();
        widest = appended > widest ? appended : widest;
        expect(appended <= 8, "burst: the cap of 8 mutations holds");
    }
    expect(widest > 1,
           "burst: a continuation probability above 0 reaches k > 1");
}

// -- the no-draw discipline ---------------------------------------------------

// Each key must draw only above its no-op value. The assertion is that the
// stream *differs* rather than that it lengthens: a structural operator returns
// as soon as it fires, skipping the side draw, the slot draw and the rewrite
// draws an ordinary mutation makes, so enabling one can shorten the stream as
// easily as lengthen it. Direction is not the property; conditionality is.
//
// That the count at the no-op values is what it was before these keys existed
// is pinned separately, by the absolute golden in
// test_zero_probability_costs_no_draw over in monotone_tests.cpp, which holds
// all four of them at no-op.
void test_each_key_draws_only_when_armed() {
    const tlsf::Specification bare_spec = three_input_spec();
    const tlsf::Specification with_assumptions = tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { b1; b2; } "
        "OUTPUTS { f1; }\nASSUME { G (F b1); G (F b2); }\n"
        "GUARANTEE { G (b1 -> F f1); }\n}\n");

    // The width and the bare form are reached only through an appended
    // assumption, so the append has to be certain for either to draw at all.
    Config appending;
    appending.p_add_assumption = 1.0;
    appending.tlsf_max_assumption_width = 1;
    appending.tlsf_p_bare_assumption = 0.0;
    appending.tlsf_p_remove_assumption = 0.0;
    appending.tlsf_p_burst_continue = 0.0;
    const std::size_t append_baseline = draws_for(bare_spec, appending, 16);

    Config wide = appending;
    wide.tlsf_max_assumption_width = 3;
    expect(draws_for(bare_spec, wide, 16) != append_baseline,
           "no-op: max_assumption_width draws only above 1");

    Config bare = appending;
    bare.p_conditional_assumption = 0.0;
    const std::size_t bare_baseline = draws_for(bare_spec, bare, 16);
    Config bare_armed = bare;
    bare_armed.tlsf_p_bare_assumption = 0.5;
    expect(draws_for(bare_spec, bare_armed, 16) != bare_baseline,
           "no-op: p_bare_assumption draws only above 0");

    // Removal needs a specification that has an assumption to remove, and the
    // append path off so the two structural branches do not shadow each other.
    Config removing;
    removing.p_add_assumption = 0.0;
    removing.p_remove_guarantee = 0.0;
    removing.tlsf_p_burst_continue = 0.0;
    removing.tlsf_p_remove_assumption = 0.0;
    const std::size_t remove_baseline =
        draws_for(with_assumptions, removing, 16);
    Config removing_armed = removing;
    removing_armed.tlsf_p_remove_assumption = 0.5;
    expect(draws_for(with_assumptions, removing_armed, 16) != remove_baseline,
           "no-op: p_remove_assumption draws only above 0");

    Config bursting;
    bursting.tlsf_p_burst_continue = 0.0;
    const std::size_t burst_baseline =
        draws_for(with_assumptions, bursting, 16);
    Config bursting_armed = bursting;
    bursting_armed.tlsf_p_burst_continue = 0.5;
    expect(draws_for(with_assumptions, bursting_armed, 16) != burst_baseline,
           "no-op: p_burst_continue draws only above 0");
}

}  // namespace

void run_tlsf_assumption_tests() {
    test_width_one_draws_a_single_literal();
    test_wide_bodies_reach_both_connectives();
    test_wide_bodies_use_inputs_only();
    test_bare_assumption_is_reachable();
    test_remove_assumption_tombstones_in_place();
    test_remove_assumption_has_no_floor();
    test_burst_zero_applies_one_mutation();
    test_burst_reaches_several_and_stops();
    test_each_key_draws_only_when_armed();
}
