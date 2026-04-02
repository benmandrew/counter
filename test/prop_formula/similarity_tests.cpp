#include <cmath>

#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

namespace {

void test_formula_syntactic_similarity_identical_formulas() {
    const Formula formula("P -> Q");
    const Formula other_formula("P -> Q");
    const size_t shared = formula.shared_subformulae(other_formula);
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
    const size_t shared = formula.shared_subformulae(other_formula);
    expect(shared == 1,
           "formula-syntactic-similarity: 'P & Q' and 'P & R' should share "
           "only one subformula");
}

void test_formula_syntactic_similarity_partial_overlap() {
    const Formula formula("~P");
    const Formula other_formula("P");
    const double synsim = formula.syntactic_similarity(other_formula);
    expect(std::fabs(synsim - 0.75) < 1e-12,
           "formula-syntactic-similarity: '~P' and 'P' should have syntactic "
           "similarity of 0.75");
}

void test_formula_shared_subformulae_counts_repeated_subformulae() {
    const Formula formula("(P & P) & P");
    const Formula other_formula("(P & P) & Q");
    const size_t shared = formula.shared_subformulae(other_formula);
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
}
