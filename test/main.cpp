#include <exception>
#include <iostream>

#include "test_suite.hpp"

int main() {
    try {
        run_transfer_system_tests();
        run_transfer_matrix_tests();
        run_ganak_runner_tests();
        run_formula_dimacs_tests();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
