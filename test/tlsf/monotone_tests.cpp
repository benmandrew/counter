#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "config.hpp"
#include "genetic/random_source.hpp"
#include "prop_formula.hpp"
#include "runner/black.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"
#include "tlsf/mutation.hpp"
#include "tlsf/parser.hpp"
#include "tlsf/specification.hpp"

namespace {

const std::vector<std::string>& atom_pool() {
    static const std::vector<std::string> pool = {"a", "b", "c"};
    return pool;
}

// Whether `from` implies `dest`, asked as the unsatisfiability of
// `from & !dest`. nullopt (a timeout, or an operator black cannot decide) is
// reported separately by the callers rather than folded into a verdict: a
// monotonicity assertion that passes on an unanswered query asserts nothing.
std::optional<bool> implies(const Formula& from, const Formula& dest) {
    SatisfiabilityChecker& checker = global_sat_checker();
    const std::string query =
        "(" + from.to_string() + ") & !(" + dest.to_string() + ")";
    const std::optional<bool> sat =
        checker.check_satisfiability(query, QueryPolarity::ExpectUnsat);
    if (!sat.has_value()) {
        return std::nullopt;
    }
    return !*sat;
}

Formula formula_of(const std::string& text) {
    const tlsf::Specification spec = tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { a; b; } "
        "OUTPUTS { c; }\nGUARANTEE { " +
        text + " }\n}\n");
    return spec.m_guarantee.front().m_formula;
}

// Hand-built subjects covering every node kind a rule fires at, plus the two
// polarity flips (a negation, and the antecedent of an implication) and a
// biconditional, whose children are sites for nothing.
std::vector<Formula> subjects() {
    return {
        formula_of("G (a -> c);"),
        formula_of("G ((a & b) -> F c);"),
        formula_of("(a | b) U c;"),
        formula_of("a W (b & c);"),
        formula_of("G (!(a & b) | c);"),
        formula_of("F (a <-> c);"),
        formula_of("G F c;"),
        formula_of("a;"),
    };
}

// The whole point of the arm: whichever node and rule are drawn, the result
// must sit on the same side of the implication order every time. A weakening
// that only usually weakens is the general rewriter with extra steps.
// Both settings of tlsf_monotone_atom_rules are run: the gate widens the rule
// menu, and a wider menu that could break monotonicity would defeat the arm.
// The guard counts per setting, so neither arm can pass on unanswered queries.
void test_monotone_rewrite_direction_holds(MonotoneDirection direction,
                                           bool atom_rules) {
    const bool weaken = direction == MonotoneDirection::Weaken;
    const char* label =
        weaken ? "monotone: parent implies the weakened rewrite"
               : "monotone: the strengthened rewrite implies its parent";
    std::size_t answered = 0;
    for (const Formula& parent : subjects()) {
        for (std::size_t seed = 0; seed < 12; ++seed) {
            const RandomSource rng = make_random_source_from_seed(seed);
            const Formula child = tlsf_monotone_rewrite(
                parent, direction, atom_rules, atom_pool(), rng);
            const std::optional<bool> held =
                weaken ? implies(parent, child) : implies(child, parent);
            if (!held.has_value()) {
                continue;
            }
            ++answered;
            expect(*held, label);
        }
    }
    // Guards against the suite passing because every query went unanswered.
    expect(answered > 32,
           "monotone: the implication oracle settled most of the queries");
}

void test_monotone_rewrite_weakens() {
    test_monotone_rewrite_direction_holds(MonotoneDirection::Weaken, false);
    test_monotone_rewrite_direction_holds(MonotoneDirection::Weaken, true);
}

void test_monotone_rewrite_strengthens() {
    test_monotone_rewrite_direction_holds(MonotoneDirection::Strengthen, false);
    test_monotone_rewrite_direction_holds(MonotoneDirection::Strengthen, true);
}

// Weakening a biconditional to one of its implications is what puts
// ltl2dba-r-2's sole ideal in reach in a single move, where the temporal
// rewrite reaches it only by regenerating both children as well.
void test_monotone_rewrite_reaches_the_biconditional_weakening() {
    const Formula parent = formula_of("G (c <-> a);");
    const Formula target = formula_of("G (c -> a);");
    bool reached = false;
    for (std::size_t seed = 0; seed < 200 && !reached; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        reached = tlsf_monotone_rewrite(parent, MonotoneDirection::Weaken,
                                        false, atom_pool(), rng) == target;
    }
    expect(reached,
           "monotone: `<->` weakens to `->` with both children untouched");
}

// What tlsf_monotone_atom_rules buys when it is on. With it off an atom's only
// monotone move is Constant, so the whole weakening menu at a literal is
// `a -> true`, and growing a literal into a disjunction -- the shape every
// assumption-shaped ideal in the corpus is built from -- is reachable only
// where a disjunction already stands.
void test_monotone_rewrite_grows_an_atom() {
    const Formula parent = formula_of("a;");
    bool weakened = false;
    bool strengthened = false;
    for (std::size_t seed = 0; seed < 200; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const Formula child = tlsf_monotone_rewrite(
            parent, MonotoneDirection::Weaken, true, atom_pool(), rng);
        if (child.kind() == Formula::Kind::Or) {
            const auto children = child.binary_children();
            weakened = children.has_value() && children->first == parent;
            if (weakened) {
                break;
            }
        }
    }
    for (std::size_t seed = 0; seed < 200; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const Formula child = tlsf_monotone_rewrite(
            parent, MonotoneDirection::Strengthen, true, atom_pool(), rng);
        if (child.kind() == Formula::Kind::And) {
            const auto children = child.binary_children();
            strengthened = children.has_value() && children->first == parent;
            if (strengthened) {
                break;
            }
        }
    }
    expect(weakened, "monotone: an atom weakens to `a | l`");
    expect(strengthened, "monotone: an atom strengthens to `a & l`");
}

// AddOperand takes its connective from the direction, not from the node it
// fires at. Reading it off the node was equivalent only while the rule was
// offered at And and Or alone, where the two agree; at any other kind, and at
// an And node being weakened, they disagree.
void test_add_operand_follows_the_direction_not_the_node() {
    const Formula parent = formula_of("(a & b);");
    bool disjoined = false;
    for (std::size_t seed = 0; seed < 200 && !disjoined; ++seed) {
        const RandomSource rng = make_random_source_from_seed(seed);
        const Formula child = tlsf_monotone_rewrite(
            parent, MonotoneDirection::Weaken, true, atom_pool(), rng);
        const auto children = child.binary_children();
        disjoined = child.kind() == Formula::Kind::Or && children.has_value() &&
                    children->first == parent;
    }
    expect(disjoined,
           "monotone: weakening a conjunction adds a disjunct, not a conjunct");
}

// The property the gate exists to hold. With tlsf_monotone_atom_rules off an
// Atom offers Constant alone, so every rewrite of a literal is `true` or
// `false` and the menu size stays 1 -- which is what makes next_index draw the
// same value it drew before the key existed, and every draw after it follow.
// A rule leaking into the off arm shows up here as a literal that grew.
void test_atom_rules_off_leaves_an_atom_ungrown() {
    const Formula parent = formula_of("a;");
    for (std::size_t seed = 0; seed < 200; ++seed) {
        for (const MonotoneDirection direction :
             {MonotoneDirection::Weaken, MonotoneDirection::Strengthen}) {
            const RandomSource rng = make_random_source_from_seed(seed);
            const Formula child = tlsf_monotone_rewrite(
                parent, direction, false, atom_pool(), rng);
            const bool constant = child == Formula::true_formula ||
                                  child == Formula::false_formula;
            expect(constant,
                   "monotone: with atom rules off an atom rewrites to a "
                   "constant, got `" +
                       child.to_string() + "`");
        }
    }
}

// A zero probability must cost no draw, or every campaign archived before the
// arm existed stops reproducing against a current binary. The guard reads the
// key before touching the RandomSource, so the count below is the cost of this
// mutation as it stood before the arm; a change to it means the short circuit
// was lost, or the surrounding grammar moved and the number wants re-pinning.
void test_zero_probability_costs_no_draw() {
    constexpr std::size_t k_expected_draws = 95;
    const tlsf::Specification original = tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { a; } "
        "OUTPUTS { c; }\nGUARANTEE { G (a -> F c); }\n}\n");
    Config off;
    off.tlsf_p_monotone = 0.0;
    // Pinned rather than left to the defaults, the discipline golden_config()
    // follows in the determinism suite: this golden is an absolute draw count,
    // so an operator added later at a non-zero default moves it and the
    // failure reads as a regression in the key actually under test. Each of
    // these is a no-op at the value set here and costs no draw.
    off.tlsf_max_assumption_width = 1;
    off.tlsf_p_bare_assumption = 0.0;
    off.tlsf_p_remove_assumption = 0.0;
    off.tlsf_p_burst_continue = 0.0;
    std::size_t drawn = 0;
    for (std::size_t seed = 0; seed < 8; ++seed) {
        const auto counter = std::make_shared<std::size_t>(0);
        std::mt19937 engine(seed);
        RandomSource source([engine, counter](std::size_t bound) mutable {
            ++*counter;
            return static_cast<std::size_t>(
                bounded_uniform(engine, static_cast<std::uint32_t>(bound)));
        });
        (void)tlsf_mutate(original, source, off);
        drawn += *counter;
    }
    expect(drawn == k_expected_draws,
           "monotone: p_monotone = 0 cost " + std::to_string(drawn) +
               " draws, expected " + std::to_string(k_expected_draws));
}

// The arm is not inert when it is on: some offspring must differ from what the
// same seed produces with it off.
void test_non_zero_probability_changes_offspring() {
    const tlsf::Specification original = tlsf::parse(
        "INFO { SEMANTICS: Mealy; }\nMAIN {\nINPUTS { a; } "
        "OUTPUTS { c; }\nGUARANTEE { G (a -> F c); }\n}\n");
    Config off;
    off.tlsf_p_monotone = 0.0;
    Config armed_cfg = off;
    armed_cfg.tlsf_p_monotone = 1.0;
    bool differ = false;
    for (std::size_t seed = 0; seed < 40 && !differ; ++seed) {
        const RandomSource baseline = make_random_source_from_seed(seed);
        const RandomSource armed = make_random_source_from_seed(seed);
        differ = !(tlsf_mutate(original, baseline, off) ==
                   tlsf_mutate(original, armed, armed_cfg));
    }
    expect(differ, "monotone: a non-zero probability does change offspring");
}

}  // namespace

void run_tlsf_monotone_tests() {
    global_sat_checker().set_timeout(std::chrono::milliseconds(5000));
    test_monotone_rewrite_weakens();
    test_monotone_rewrite_strengthens();
    test_monotone_rewrite_reaches_the_biconditional_weakening();
    test_monotone_rewrite_grows_an_atom();
    test_add_operand_follows_the_direction_not_the_node();
    test_atom_rules_off_leaves_an_atom_ungrown();
    test_zero_probability_costs_no_draw();
    test_non_zero_probability_changes_offspring();
}
