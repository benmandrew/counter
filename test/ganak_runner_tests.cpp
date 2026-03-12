#include <filesystem>
#include <fstream>

#include "ganak_runner.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

void test_ganak_runner_on_trivial_cnf() {
    const std::filesystem::path temp_dir =
        std::filesystem::temp_directory_path();
    const std::filesystem::path dimacs_path =
        temp_dir / "counter-ganak-test.cnf";

    {
        std::ofstream dimacs_file(dimacs_path);
        expect(dimacs_file.good(),
               "ganak-runner: failed to create temporary DIMACS file");
        dimacs_file << "p cnf 1 1\n";
        dimacs_file << "1 0\n";
    }

    const Count count = run_ganak_on_dimacs(dimacs_path.string(), 1);
    expect(count == 1,
           "ganak-runner: expected count 1 for single-literal SAT CNF");

    std::filesystem::remove(dimacs_path);
}

void run_ganak_runner_tests() { test_ganak_runner_on_trivial_cnf(); }
