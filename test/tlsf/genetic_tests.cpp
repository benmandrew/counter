#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "config.hpp"
#include "genetic/generation.hpp"
#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/crossover.hpp"
#include "tlsf/filter.hpp"
#include "tlsf/fitness.hpp"
#include "tlsf/mutation.hpp"
#include "tlsf/operators.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

tlsf::Specification parse(const std::string& main_body,
                          const std::string& semantics = "Mealy") {
    return tlsf::parse("INFO { SEMANTICS: " + semantics + "; }\nMAIN {\n" +
                       main_body + "\n}\n");
}

bool is_temporal(Formula::Kind kind) {
    switch (kind) {
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally:
        case Formula::Kind::Until:
        case Formula::Kind::Release:
        case Formula::Kind::WeakUntil:
            return true;
        default:
            return false;
    }
}

// Multiset of temporal operator kinds encountered in a formula, used to assert
// the temporal skeleton is preserved under mutation.
void collect_temporal(const Formula& formula, std::multiset<int>& out) {
    if (is_temporal(formula.kind())) {
        out.insert(static_cast<int>(formula.kind()));
    }
    switch (formula.kind()) {
        case Formula::Kind::Atom:
            break;
        case Formula::Kind::Not:
        case Formula::Kind::Next:
        case Formula::Kind::Eventually:
        case Formula::Kind::Globally: {
            const auto child = formula.unary_child();
            if (child.has_value()) {
                collect_temporal(*child, out);
            }
            break;
        }
        default: {
            const auto children = formula.binary_children();
            if (children.has_value()) {
                collect_temporal(children->first, out);
                collect_temporal(children->second, out);
            }
            break;
        }
    }
}

std::multiset<int> temporal_kinds(const Formula& formula) {
    std::multiset<int> kinds;
    collect_temporal(formula, kinds);
    return kinds;
}

void test_mutation_preserves_temporal_skeleton() {
    Config cfg;
    cfg.tlsf_p_temporal = 0.0;  // isolate the skeleton-preserving rewrite path
    const tlsf::Specification original = parse(
        "INPUTS { req; } OUTPUTS { grant; } GUARANTEE { G(req -> F "
        "grant); }");
    const std::multiset<int> skeleton =
        temporal_kinds(original.m_guarantee.front());
    expect(!original.m_guarantee.front().is_propositional(),
           "mutation: the seed formula is genuinely temporal");

    for (std::size_t seed = 0; seed < 40; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(original, rng, cfg);
        expect(mutated.m_guarantee.size() == 1,
               "mutation: guarantee section shape is preserved");
        const Formula& formula = mutated.m_guarantee.front();
        expect(!formula.is_propositional(),
               "mutation: the temporal structure survives mutation");
        expect(temporal_kinds(formula) == skeleton,
               "mutation: the multiset of temporal operators is unchanged");
        expect(!formula.to_string().empty(),
               "mutation: mutated formula has a well-formed string form");
    }
}

void test_mutation_assumption_atoms_from_inputs_only() {
    Config cfg;
    cfg.p_add_assumption =
        0.0;  // isolate the rewrite path (not add-assumption)
    tlsf::Specification spec;
    spec.m_inputs = {"a", "c"};
    spec.m_outputs = {"bout"};
    spec.m_assume = {Formula("a")};
    // No guarantee-side formulae, so every mutation falls to the assumption
    // side and must draw atoms from the inputs only.
    for (std::size_t seed = 0; seed < 40; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        expect(mutated.m_assume.size() == 1,
               "mutation: assumption section shape is preserved");
        const std::string text = mutated.m_assume.front().to_string();
        expect(text.find("bout") == std::string::npos,
               "mutation: the output atom never leaks into an assumption");
    }
}

void test_temporal_mutation_changes_skeleton() {
    // With tlsf_p_temporal forced to 1, the chosen formula is rewritten by the
    // Brizzio-style operator, which is allowed to insert/drop/swap temporal
    // operators. Over a range of seeds the temporal skeleton must actually
    // change at least once, and every result must stay well-formed.
    Config cfg;
    cfg.tlsf_p_temporal = 1.0;
    cfg.p_add_assumption = 0.0;  // isolate the rewrite path
    const tlsf::Specification original = parse(
        "INPUTS { req; } OUTPUTS { grant; } GUARANTEE { G(req -> F "
        "grant); }");
    const std::multiset<int> skeleton =
        temporal_kinds(original.m_guarantee.front());

    bool skeleton_changed = false;
    for (std::size_t seed = 0; seed < 40; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(original, rng, cfg);
        expect(mutated.m_guarantee.size() == 1,
               "temporal mutation: guarantee section shape is preserved");
        const Formula& formula = mutated.m_guarantee.front();
        expect(!formula.to_string().empty(),
               "temporal mutation: mutated formula has a well-formed string "
               "form");
        if (temporal_kinds(formula) != skeleton) {
            skeleton_changed = true;
        }
    }
    expect(skeleton_changed,
           "temporal mutation: the temporal skeleton is altered for at least "
           "one seed");
}

void test_temporal_mutation_atoms_from_inputs_only() {
    // The temporal operator threads the side-appropriate atom pool through its
    // recursion, so an assumption-side rewrite must never draw an output atom.
    Config cfg;
    cfg.tlsf_p_temporal = 1.0;
    cfg.p_add_assumption = 0.0;
    tlsf::Specification spec;
    spec.m_inputs = {"a", "c"};
    spec.m_outputs = {"bout"};
    spec.m_assume = {parse("INPUTS { a; c; } OUTPUTS { bout; } "
                           "ASSUME { G(a -> X c); }")
                         .m_assume.front()};
    for (std::size_t seed = 0; seed < 40; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        expect(mutated.m_assume.size() == 1,
               "temporal mutation: assumption section shape is preserved");
        const std::string text = mutated.m_assume.front().to_string();
        expect(text.find("bout") == std::string::npos,
               "temporal mutation: the output atom never leaks into an "
               "assumption");
    }
}

void test_add_assumption_appends_fairness() {
    // With p_add_assumption forced to 1, mutation appends a fairness assumption
    // `G F <input>` (input optionally negated) to the ASSUME section and leaves
    // the rest of the specification untouched.
    tlsf::Specification spec;
    spec.m_inputs = {"req"};
    spec.m_outputs = {"grant"};
    spec.m_guarantee = {parse("INPUTS { req; } OUTPUTS { grant; } "
                              "GUARANTEE { G (req -> F grant); }")
                            .m_guarantee.front()};
    Config cfg;
    cfg.p_add_assumption = 1.0;
    for (std::size_t seed = 0; seed < 20; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        expect(mutated.m_assume.size() == 1,
               "add-assumption: exactly one assumption is appended");
        expect(mutated.m_guarantee == spec.m_guarantee,
               "add-assumption: guarantees are left untouched");
        const std::string text = mutated.m_assume.front().to_string();
        expect(text == "G(F(req))" || text == "G(F(!(req)))",
               "add-assumption: appended a G F <input> fairness assumption");
        expect(text.find("grant") == std::string::npos,
               "add-assumption: an output signal never enters an assumption");
    }
}

void test_assumption_rewrite_can_reference_output_when_allowed() {
    // The companion to test_mutation_assumption_atoms_from_inputs_only: with
    // allow_output_assumptions set, an assumption-side *rewrite* (not just the
    // add-assumption action) may draw an output atom, so an output-referencing
    // assumption can be reshaped instead of having its output overwritten. This
    // is what lets a G F <output> or G(c -> F <output>) grow a weak-until
    // hold-until form over successive generations.
    Config cfg;
    cfg.p_add_assumption = 0.0;  // isolate the rewrite path
    cfg.allow_output_assumptions = true;
    tlsf::Specification spec;
    spec.m_inputs = {"a", "c"};
    spec.m_outputs = {"bout"};
    spec.m_assume = {Formula("a")};
    // No guarantee-side formulae, so every mutation falls to the assumption
    // side; with the flag set its atom pool now includes the outputs.
    bool saw_output = false;
    for (std::size_t seed = 0; seed < 60; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        if (mutated.m_assume.front().to_string().find("bout") !=
            std::string::npos) {
            saw_output = true;
        }
    }
    expect(saw_output,
           "mutation: an assumption-side rewrite can introduce an output atom "
           "when allow_output_assumptions is set");
}

void test_weak_until_over_output_is_reachable() {
    // A weak-until (hold-until) assumption over an output does not need a
    // dedicated new operator: the temporal mutation already emits W, and with
    // allow_output_assumptions the assumption-side pool keeps the output atom
    // through a rewrite. Starting from the kind of fairness assumption
    // tlsf_add_assumption creates (G(r -> F g)), a temporal rewrite can yield
    // an assumption with a `... W ...` node referencing the output g.
    Config cfg;
    cfg.p_add_assumption = 0.0;   // isolate the rewrite path
    cfg.tlsf_p_assumption = 1.0;  // always mutate the assumption side
    cfg.tlsf_p_temporal = 1.0;    // always the temporal (skeleton) rewrite
    cfg.allow_output_assumptions = true;
    tlsf::Specification seed_spec;
    seed_spec.m_inputs = {"r"};
    seed_spec.m_outputs = {"g"};
    seed_spec.m_assume = {parse("INPUTS { r; } OUTPUTS { g; } "
                                "ASSUME { G (r -> F g); }")
                              .m_assume.front()};
    bool reached = false;
    for (std::size_t seed = 0; seed < 200 && !reached; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        tlsf::Specification current = seed_spec;
        for (int step = 0; step < 6 && !reached; ++step) {
            current = tlsf_mutate(current, rng, cfg);
            for (const Formula& formula : current.m_assume) {
                const std::string text = formula.to_string();
                if (text.find(") W (") != std::string::npos &&
                    text.find('g') != std::string::npos) {
                    reached = true;
                }
            }
        }
    }
    expect(reached,
           "mutation: a weak-until assumption over an output is reachable by "
           "temporal mutation of a fairness assumption");
}

void test_add_assumption_can_reference_output_when_allowed() {
    // With allow_output_assumptions set, the appended assumption draws from
    // inputs ∪ outputs, so the output atom is reachable over a range of seeds.
    // The well-separation filter, not a syntactic ban, is what then prunes any
    // not-well-separated result.
    tlsf::Specification spec;
    spec.m_inputs = {"req"};
    spec.m_outputs = {"grant"};
    spec.m_guarantee = {parse("INPUTS { req; } OUTPUTS { grant; } "
                              "GUARANTEE { G (req -> F grant); }")
                            .m_guarantee.front()};
    Config cfg;
    cfg.p_add_assumption = 1.0;
    cfg.allow_output_assumptions = true;
    bool saw_output = false;
    for (std::size_t seed = 0; seed < 60; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const tlsf::Specification mutated = tlsf_mutate(spec, rng, cfg);
        expect(mutated.m_assume.size() == 1,
               "add-assumption(output): exactly one assumption is appended");
        if (mutated.m_assume.front().to_string().find("grant") !=
            std::string::npos) {
            saw_output = true;
        }
    }
    expect(saw_output,
           "add-assumption(output): the output atom is reachable in an "
           "assumption when allow_output_assumptions is set");
}

void test_crossover_positional_matching_shape() {
    tlsf::Specification parent_a;
    parent_a.m_inputs = {"r"};
    parent_a.m_outputs = {"g"};
    parent_a.m_guarantee = {Formula("r"), Formula("g")};
    tlsf::Specification parent_b = parent_a;
    parent_b.m_guarantee = {Formula("!r"), Formula("!g")};

    const RandomSource rng = make_random_source_from_seed(7);
    const tlsf::Specification child = tlsf_crossover(parent_a, parent_b, rng);
    expect(child.m_guarantee.size() == 2,
           "crossover: section sizes are preserved");
    for (std::size_t i = 0; i < child.m_guarantee.size(); ++i) {
        const Formula& picked = child.m_guarantee[i];
        expect(picked == parent_a.m_guarantee[i] ||
                   picked == parent_b.m_guarantee[i],
               "crossover: each formula comes from one of the two parents");
    }
    expect(child.m_inputs == parent_a.m_inputs &&
               child.m_outputs == parent_a.m_outputs,
           "crossover: signals are inherited from the first parent");
}

void test_crossover_mismatched_shape_returns_first() {
    tlsf::Specification parent_a;
    parent_a.m_inputs = {"r"};
    parent_a.m_outputs = {"g"};
    parent_a.m_guarantee = {Formula("r")};
    tlsf::Specification parent_b = parent_a;
    parent_b.m_guarantee = {Formula("!r"), Formula("g")};

    const RandomSource rng = make_random_source_from_seed(1);
    const tlsf::Specification child = tlsf_crossover(parent_a, parent_b, rng);
    expect(child == parent_a,
           "crossover: mismatched section shapes return the first parent "
           "unchanged");
}

void test_end_to_end_evolution() {
    Config cfg;
    cfg.population_size = 4;
    cfg.default_model_counting_bound = 3;
    cfg.parallel = 1;
    cfg.crossover_rate = 0.5;
    cfg.mutation_rate = 1.0;

    // A Mealy spec requiring the output to predict the next input:
    // unrealizable.
    const tlsf::Specification original =
        parse("INPUTS { r; } OUTPUTS { g; } GUARANTEE { G(g <-> X r); }");
    const auto fitness = tlsf_get_fitness_function(original, cfg);

    const std::size_t target_size = 4;
    const std::vector<tlsf::Specification> seed_pop(target_size, original);
    std::vector<Scored<tlsf::Specification>> population =
        score_population(cfg, seed_pop, fitness);

    const std::vector<FilterFunctionT<tlsf::Specification>> filters = {
        tlsf_make_dedup_filter(), tlsf_make_assumption_sat_filter()};

    for (std::size_t generation = 0; generation < 2; ++generation) {
        const RandomSource rng = make_random_source_from_seed(generation + 1);
        population = evolve_generation_generic(cfg, population, target_size,
                                               /*elitism_size=*/0, fitness,
                                               filters, tlsf_operators(), rng);
        expect(population.size() == target_size,
               "end-to-end: each generation returns target_size offspring");
        for (const Scored<tlsf::Specification>& scored : population) {
            expect(
                std::isfinite(scored.fitness) && scored.fitness >= 0.0 &&
                    scored.fitness <= 1.0,
                "end-to-end: every offspring has a finite fitness in [0, 1]");
        }
    }
}

}  // namespace

void run_tlsf_genetic_tests() {
    test_mutation_preserves_temporal_skeleton();
    test_mutation_assumption_atoms_from_inputs_only();
    test_temporal_mutation_changes_skeleton();
    test_temporal_mutation_atoms_from_inputs_only();
    test_add_assumption_appends_fairness();
    test_add_assumption_can_reference_output_when_allowed();
    test_assumption_rewrite_can_reference_output_when_allowed();
    test_weak_until_over_output_is_reachable();
    test_crossover_positional_matching_shape();
    test_crossover_mismatched_shape_returns_first();
    test_end_to_end_evolution();
}
