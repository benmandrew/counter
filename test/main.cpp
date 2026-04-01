#include <exception>
#include <iostream>

#include "test_suite.hpp"

int main() {
    try {
        run_transfer_matrix_tests();
        run_ganak_runner_tests();
        run_prop_formula_tests();
        run_semantic_similarity_tests();
        run_syntactic_similarity_tests();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}
