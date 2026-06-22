#include <cmath>

#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_formula_syntactic_similarity_identical_formulas() {
    const Formula formula("P -> Q");
    const Formula other_formula("P -> Q");
    const std::size_t shared = formula.shared_subformulae(other_formula);
    expect(shared == 3,
           "formula-syntactic-similarity: identical 'P -> Q' should share 3 "
           "subformulae including the root");
    const double synsim = formula.syntactic_similarity(other_formula);
    expect(
        std::fabs(synsim - 1.0) < 1e-12,
        "formula-syntactic-similarity: identical 'P -> Q' should have perfect "
        "syntactic similarity");
}

void test_formula_shared_subformulae_partial_overlap() {
    const Formula formula("P & Q");
    const Formula other_formula("P & R");
    const std::size_t shared = formula.shared_subformulae(other_formula);
    expect(shared == 1,
           "formula-syntactic-similarity: 'P & Q' and 'P & R' should share "
           "only one subformula");
}

void test_formula_syntactic_similarity_partial_overlap() {
    const Formula formula("~P");
    const Formula other_formula("P");
    const double synsim = formula.syntactic_similarity(other_formula);
    // shared=1, n(~P)=2, n(P)=1: first=0.5, second=1.0,
    // harmonic mean = 2*0.5*1.0/1.5 = 2/3.
    expect(std::fabs(synsim - (2.0 / 3.0)) < 1e-12,
           "formula-syntactic-similarity: '~P' and 'P' should have syntactic "
           "similarity of 2/3");
}

// A formula that is a literal subformula of a much larger one would float at
// (1.0 + tiny) / 2 ~= 0.5 under an arithmetic mean of the two containment
// ratios, regardless of how much bigger the other formula is. The harmonic
// mean lets the small ratio pull the score down instead.
void test_formula_syntactic_similarity_small_subformula_of_large_formula() {
    const Formula formula("P");
    const Formula other_formula("P & Q & R & S & T");
    const double synsim = formula.syntactic_similarity(other_formula);
    // shared=1, n(P)=1, n(other)=9 (5 atoms, 4 And nodes): first=1.0,
    // second=1/9, harmonic mean = 2*1*(1/9)/(1+1/9) = 0.2.
    expect(std::fabs(synsim - 0.2) < 1e-12,
           "formula-syntactic-similarity: a small subformula of a much "
           "larger formula should score well below the old 0.5 floor");
}

void test_formula_shared_subformulae_counts_repeated_subformulae() {
    const Formula formula("(P & P) & P");
    const Formula other_formula("(P & P) & Q");
    const std::size_t shared = formula.shared_subformulae(other_formula);
    expect(shared == 3,
           "formula-shared-subformulae: repeated subformulae should "
           "be counted with multiplicity");
}

}  // namespace

void run_prop_formula_similarity_tests() {
    test_formula_syntactic_similarity_identical_formulas();
    test_formula_shared_subformulae_partial_overlap();
    test_formula_shared_subformulae_counts_repeated_subformulae();
    test_formula_syntactic_similarity_partial_overlap();
    test_formula_syntactic_similarity_small_subformula_of_large_formula();
}
