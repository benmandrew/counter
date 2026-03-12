#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "formula_dimacs.hpp"
#include "ganak_runner.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

void test_formula_to_dimacs_implies_count() {
    const DimacsCnf cnf = formula_to_dimacs("P -> Q");

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path();
    const std::filesystem::path dimacs_path =
        temp_dir / "counter-formula-implies.cnf";

    {
        std::ofstream dimacs_file(dimacs_path);
        expect(dimacs_file.good(),
               "formula-dimacs: failed to create DIMACS file for implies");
        dimacs_file << cnf.to_dimacs();
    }

    const Count count = run_ganak_on_dimacs(dimacs_path.string(), 1);
    expect(count == 3,
           "formula-dimacs: expected 3 models for formula 'P -> Q'");

    std::filesystem::remove(dimacs_path);
}

void test_formula_to_dimacs_precedence_count() {
    const DimacsCnf cnf = formula_to_dimacs("A | B & C");

    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path();
    const std::filesystem::path dimacs_path =
        temp_dir / "counter-formula-precedence.cnf";

    {
        std::ofstream dimacs_file(dimacs_path);
        expect(dimacs_file.good(),
               "formula-dimacs: failed to create DIMACS file for precedence");
        dimacs_file << cnf.to_dimacs();
    }

    const Count count = run_ganak_on_dimacs(dimacs_path.string(), 1);
    expect(count == 5,
           "formula-dimacs: expected 5 models for formula 'A | B & C'");

    std::filesystem::remove(dimacs_path);
}

void test_formula_to_dimacs_rejects_invalid_formula() {
    bool threw = false;
    try {
        (void)formula_to_dimacs("P &");
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    expect(threw, "formula-dimacs: invalid formula should throw");
}

void run_formula_dimacs_tests() {
    test_formula_to_dimacs_implies_count();
    test_formula_to_dimacs_precedence_count();
    test_formula_to_dimacs_rejects_invalid_formula();
}
