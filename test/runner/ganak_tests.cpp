#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "runner/ganak.hpp"
#include "test_suite.hpp"
#include "test_support.hpp"

void test_ganak_runner_on_trivial_cnf() {
    char dimacs_path[] = "/tmp/counter-ganak-test-XXXXXX";
    const int file_descriptor = mkstemp(dimacs_path);
    expect(file_descriptor >= 0,
           "ganak-runner: failed to create temporary DIMACS file");
    close(file_descriptor);

    {
        std::ofstream dimacs_file(dimacs_path);
        expect(dimacs_file.good(),
               "ganak-runner: failed to create temporary DIMACS file");
        dimacs_file << "p cnf 1 1\n";
        dimacs_file << "1 0\n";
    }

    const Count count = run_ganak_on_dimacs(dimacs_path, 1);
    expect(count == 1,
           "ganak-runner: expected count 1 for single-literal SAT CNF");

    std::remove(dimacs_path);
}

void run_ganak_runner_tests() { test_ganak_runner_on_trivial_cnf(); }
