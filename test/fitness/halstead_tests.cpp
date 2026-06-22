#include <algorithm>
#include <cmath>

#include "fitness/halstead.hpp"
#include "prop_formula.hpp"
#include "requirement.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

// --- halstead_counts(Formula) ---

void test_counts_single_atom() {
    const HalsteadCounts counts = halstead_counts(Formula("p"));
    expect(counts.eta1 == 0, "counts/atom: eta1 should be 0");
    expect(counts.eta2 == 1, "counts/atom: eta2 should be 1");
    expect(counts.n1 == 0, "counts/atom: n1 should be 0");
    expect(counts.n2 == 1, "counts/atom: n2 should be 1");
}

void test_counts_negation() {
    // !p: one operator (!), one operand (p)
    const HalsteadCounts counts = halstead_counts(Formula("!p"));
    expect(counts.eta1 == 1, "counts/neg: eta1 should be 1");
    expect(counts.eta2 == 1, "counts/neg: eta2 should be 1");
    expect(counts.n1 == 1, "counts/neg: n1 should be 1");
    expect(counts.n2 == 1, "counts/neg: n2 should be 1");
}

void test_counts_conjunction_distinct_atoms() {
    // p & q: one operator (&), two distinct operands
    const HalsteadCounts counts = halstead_counts(Formula("p & q"));
    expect(counts.eta1 == 1, "counts/and: eta1 should be 1");
    expect(counts.eta2 == 2, "counts/and: eta2 should be 2");
    expect(counts.n1 == 1, "counts/and: n1 should be 1");
    expect(counts.n2 == 2, "counts/and: n2 should be 2");
}

void test_counts_repeated_atom() {
    // p & p: one distinct operator, one distinct operand, but n2=2 (p appears
    // twice)
    const HalsteadCounts counts = halstead_counts(Formula("p & p"));
    expect(counts.eta1 == 1, "counts/repeated: eta1 should be 1");
    expect(counts.eta2 == 1,
           "counts/repeated: eta2 should be 1 (p is one distinct operand)");
    expect(counts.n1 == 1, "counts/repeated: n1 should be 1");
    expect(counts.n2 == 2, "counts/repeated: n2 should be 2 (p counted twice)");
}

void test_counts_nested() {
    // !p & q: two distinct operators (!, &), two distinct operands (p, q)
    const HalsteadCounts counts = halstead_counts(Formula("!p & q"));
    expect(counts.eta1 == 2, "counts/nested: eta1 should be 2");
    expect(counts.eta2 == 2, "counts/nested: eta2 should be 2");
    expect(counts.n1 == 2, "counts/nested: n1 should be 2");
    expect(counts.n2 == 2, "counts/nested: n2 should be 2");
}

void test_counts_all_connectives() {
    // Each distinct connective counted once in eta1
    const HalsteadCounts neg = halstead_counts(Formula("!p"));
    const HalsteadCounts conj = halstead_counts(Formula("p & q"));
    const HalsteadCounts disj = halstead_counts(Formula("p | q"));
    const HalsteadCounts impl = halstead_counts(Formula("p -> q"));
    const HalsteadCounts iff = halstead_counts(Formula("p <-> q"));
    expect(neg.eta1 == 1 && conj.eta1 == 1 && disj.eta1 == 1 &&
               impl.eta1 == 1 && iff.eta1 == 1,
           "counts/connectives: each connective should give eta1==1");
}

// --- halstead_volume ---

void test_volume_single_atom_is_zero() {
    // eta = 0+1 = 1 → log2(1) = 0
    const double vol = halstead_volume(halstead_counts(Formula("p")));
    expect(vol == 0.0, "volume/atom: volume of a single atom should be 0");
}

void test_volume_negation_exact() {
    // !p: eta=2, N=2 → 2*log2(2) = 2.0
    const double vol = halstead_volume(halstead_counts(Formula("!p")));
    expect(std::fabs(vol - 2.0) < 1e-12,
           "volume/neg: volume of !p should be exactly 2.0");
}

void test_volume_repeated_atom_exact() {
    // p & p: eta1=1, eta2=1 → eta=2; n1=1, n2=2 → N=3 → 3*log2(2) = 3.0
    const double vol = halstead_volume(halstead_counts(Formula("p & p")));
    expect(std::fabs(vol - 3.0) < 1e-12,
           "volume/repeated: volume of p&p should be 3.0");
}

void test_volume_nested_exact() {
    // !p & q: eta1=2, eta2=2 → eta=4; n1=2, n2=2 → N=4 → 4*log2(4) = 8.0
    const double vol = halstead_volume(halstead_counts(Formula("!p & q")));
    expect(std::fabs(vol - 8.0) < 1e-12,
           "volume/nested: volume of !p&q should be exactly 8.0");
}

void test_volume_increases_with_complexity() {
    const double vol_atom = halstead_volume(halstead_counts(Formula("p")));
    const double vol_neg = halstead_volume(halstead_counts(Formula("!p")));
    const double vol_and = halstead_volume(halstead_counts(Formula("p & q")));
    const double vol_nested =
        halstead_volume(halstead_counts(Formula("!p & q")));
    expect(vol_atom < vol_neg,
           "volume/ordering: !p should have greater volume than p");
    expect(vol_neg < vol_and,
           "volume/ordering: p&q should have greater volume than !p");
    expect(vol_and < vol_nested,
           "volume/ordering: !p&q should have greater volume than p&q");
}

// --- halstead_counts(Requirement) ---

void test_counts_req_timing_contributes_operator() {
    // Timing adds an operator; non-parameterised adds no operand.
    const Requirement req_imm{Formula("p"), Formula("q"),
                              timing::immediately()};
    const Requirement req_ev{Formula("p"), Formula("q"), timing::eventually()};
    const HalsteadCounts imm = halstead_counts(req_imm);
    const HalsteadCounts evn = halstead_counts(req_ev);
    // Both have same formulas; their timing operators are distinct strings so
    // eta1 should be 1 for each (only the timing operator; no connectives).
    expect(imm.eta1 == 1,
           "counts/req-timing: immediately should contribute 1 operator");
    expect(evn.eta1 == 1,
           "counts/req-timing: eventually should contribute 1 operator");
    // eta1 should differ (different timing string), but each alone gives 1
    // distinct operator — combined counts for a SINGLE requirement match.
    expect(imm.n1 == 1, "counts/req-timing: n1 should be 1 for immediately");
    expect(evn.n1 == 1, "counts/req-timing: n1 should be 1 for eventually");
}

void test_counts_req_parameterised_timing_contributes_operand() {
    // within_ticks(5) adds operator "within" AND operand "ticks:5"
    const Requirement req{Formula("p"), Formula("q"), timing::within_ticks(5)};
    const HalsteadCounts counts = halstead_counts(req);
    // Operators: {"within"} → eta1=1
    // Operands: {"p","q","ticks:5"} → eta2=3
    expect(counts.eta1 == 1,
           "counts/req-param: parameterised timing should add 1 operator");
    expect(counts.eta2 == 3,
           "counts/req-param: parameterised timing should add tick count as "
           "operand (eta2=3)");
    expect(counts.n2 == 3, "counts/req-param: n2 should be 3 (p, q, ticks:5)");
}

void test_counts_req_different_tick_values_are_distinct_operands() {
    // within_ticks(3) and within_ticks(5) share the operator but have
    // different tick-count operands.
    const Requirement req3{Formula("p"), Formula("q"), timing::within_ticks(3)};
    const Requirement req5{Formula("p"), Formula("q"), timing::within_ticks(5)};
    const HalsteadCounts cnt3 = halstead_counts(req3);
    const HalsteadCounts cnt5 = halstead_counts(req5);
    expect(cnt3.eta2 == 3,
           "counts/tick-operands: within_ticks(3) should have 3 distinct "
           "operands (p,q,ticks:3)");
    expect(cnt5.eta2 == 3,
           "counts/tick-operands: within_ticks(5) should have 3 distinct "
           "operands (p,q,ticks:5)");
}

// --- halstead_counts(Specification) ---

void test_counts_spec_shares_operators_across_requirements() {
    // Two requirements both using "eventually" contribute eta1=1, not 2.
    const Specification spec(
        {},
        {Requirement{Formula("p"), Formula("q"), timing::eventually()},
         Requirement{Formula("r"), Formula("s"), timing::eventually()}},
        {}, {});
    const HalsteadCounts counts = halstead_counts(spec);
    expect(counts.eta1 == 1,
           "counts/spec: shared timing operator should appear once in eta1");
    expect(counts.eta2 == 4,
           "counts/spec: four distinct atoms across two requirements");
    expect(
        counts.n1 == 2,
        "counts/spec: two timing operator occurrences (one per requirement)");
    expect(counts.n2 == 4, "counts/spec: four total atom occurrences");
}

void test_counts_spec_mixed_timings() {
    // One eventually, one immediately → two distinct timing operators.
    const Specification spec(
        {},
        {Requirement{Formula("p"), Formula("q"), timing::eventually()},
         Requirement{Formula("r"), Formula("s"), timing::immediately()}},
        {}, {});
    const HalsteadCounts counts = halstead_counts(spec);
    expect(counts.eta1 == 2,
           "counts/spec-mixed: two distinct timing operators");
}

// --- halstead_fitness ---

void test_fitness_identical_spec_is_one() {
    const Specification spec(
        {}, {Requirement{Formula("p"), Formula("q"), timing::eventually()}}, {},
        {});
    const double fitness = halstead_fitness(spec, spec);
    expect(std::fabs(fitness - 1.0) < 1e-12,
           "fitness: identical specs should give fitness 1.0");
}

void test_fitness_simpler_candidate_clamps_to_one() {
    // Original is more complex than candidate → clamped to 1.0.
    const Specification original(
        {},
        {Requirement{Formula("!p & q"), Formula("!r | s"),
                     timing::eventually()}},
        {}, {});
    const Specification candidate(
        {}, {Requirement{Formula("p"), Formula("q"), timing::eventually()}}, {},
        {});
    const double fitness = halstead_fitness(candidate, original);
    expect(std::fabs(fitness - 1.0) < 1e-12,
           "fitness: simpler candidate should clamp to 1.0");
}

void test_fitness_more_complex_candidate_is_less_than_one() {
    const Specification original(
        {}, {Requirement{Formula("p"), Formula("q"), timing::immediately()}},
        {}, {});
    const Specification candidate(
        {},
        {Requirement{Formula("!p & r"), Formula("q | s"),
                     timing::immediately()}},
        {}, {});
    const double fitness = halstead_fitness(candidate, original);
    expect(fitness < 1.0,
           "fitness: more complex candidate should give fitness < 1.0");
    expect(fitness > 0.0,
           "fitness: more complex candidate should give fitness > 0.0");
}

void test_fitness_equals_volume_ratio() {
    // halstead_fitness = min(1, V_orig / V_cand).
    // Verify numerically by computing volumes independently.
    const Specification original(
        {}, {Requirement{Formula("p"), Formula("q"), timing::immediately()}},
        {}, {});
    const Specification candidate(
        {},
        {Requirement{Formula("!p & r"), Formula("q | s"),
                     timing::immediately()}},
        {}, {});
    const double v_orig = halstead_volume(halstead_counts(original));
    const double v_cand = halstead_volume(halstead_counts(candidate));
    const double expected = std::min(1.0, v_orig / v_cand);
    const double fitness = halstead_fitness(candidate, original);
    expect(std::fabs(fitness - expected) < 1e-12,
           "fitness: should equal min(1, V_original/V_candidate)");
}

void test_fitness_decreases_as_complexity_grows() {
    const Specification original(
        {}, {Requirement{Formula("p"), Formula("q"), timing::immediately()}},
        {}, {});
    const Specification less_complex(
        {}, {Requirement{Formula("!p"), Formula("q"), timing::immediately()}},
        {}, {});
    const Specification more_complex(
        {},
        {Requirement{Formula("!p & r"), Formula("q | s"),
                     timing::immediately()}},
        {}, {});
    const double f_less = halstead_fitness(less_complex, original);
    const double f_more = halstead_fitness(more_complex, original);
    expect(f_less >= f_more,
           "fitness: less complex candidate should score at least as well as "
           "more complex one");
}

}  // namespace

void run_halstead_tests() {
    test_counts_single_atom();
    test_counts_negation();
    test_counts_conjunction_distinct_atoms();
    test_counts_repeated_atom();
    test_counts_nested();
    test_counts_all_connectives();
    test_volume_single_atom_is_zero();
    test_volume_negation_exact();
    test_volume_repeated_atom_exact();
    test_volume_nested_exact();
    test_volume_increases_with_complexity();
    test_counts_req_timing_contributes_operator();
    test_counts_req_parameterised_timing_contributes_operand();
    test_counts_req_different_tick_values_are_distinct_operands();
    test_counts_spec_shares_operators_across_requirements();
    test_counts_spec_mixed_timings();
    test_fitness_identical_spec_is_one();
    test_fitness_simpler_candidate_clamps_to_one();
    test_fitness_more_complex_candidate_is_less_than_one();
    test_fitness_equals_volume_ratio();
    test_fitness_decreases_as_complexity_grows();
}
