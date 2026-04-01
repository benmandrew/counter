#include <unistd.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

#include "ganak_runner.hpp"
#include "prop_formula.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

void test_formula_to_dimacs_implies_count() {
    const Formula formula = Formula("P -> Q");
    char dimacs_path[] = "/tmp/counter-formula-implies-XXXXXX";
    const int file_descriptor = mkstemp(dimacs_path);
    expect(file_descriptor >= 0,
           "formula-dimacs: failed to create DIMACS file for implies");
    close(file_descriptor);
    {
        std::ofstream dimacs_file(dimacs_path);
        expect(dimacs_file.good(),
               "formula-dimacs: failed to create DIMACS file for implies");
        dimacs_file << formula.to_dimacs();
    }
    const Count count = run_ganak_on_dimacs(dimacs_path, 1);
    expect(count == 3,
           "formula-dimacs: expected 3 models for formula 'P -> Q'");
    std::remove(dimacs_path);
}

void test_formula_to_dimacs_precedence_count() {
    const Formula formula = Formula("A | B & C");
    char dimacs_path[] = "/tmp/counter-formula-precedence-XXXXXX";
    const int file_descriptor = mkstemp(dimacs_path);
    expect(file_descriptor >= 0,
           "formula-dimacs: failed to create DIMACS file for precedence");
    close(file_descriptor);
    {
        std::ofstream dimacs_file(dimacs_path);
        expect(dimacs_file.good(),
               "formula-dimacs: failed to create DIMACS file for precedence");
        dimacs_file << formula.to_dimacs();
    }
    const Count count = run_ganak_on_dimacs(dimacs_path, 1);
    expect(count == 5,
           "formula-dimacs: expected 5 models for formula 'A | B & C'");
    std::remove(dimacs_path);
}

void test_formula_to_dimacs_rejects_invalid_formula() {
    bool threw = false;
    try {
        (void)Formula("P &");
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    expect(threw, "formula-dimacs: invalid formula should throw");
}

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

void run_prop_formula_tests() {
    test_formula_to_dimacs_implies_count();
    test_formula_to_dimacs_precedence_count();
    test_formula_to_dimacs_rejects_invalid_formula();
    test_formula_syntactic_similarity_identical_formulas();
    test_formula_shared_subformulae_partial_overlap();
    test_formula_shared_subformulae_counts_repeated_subformulae();
    test_formula_syntactic_similarity_partial_overlap();
}
