#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "test_suite.hpp"

namespace {

void run_suite(std::string_view suite_name) {
    if (suite_name == "transfer_matrix") {
        run_transfer_matrix_tests();
        return;
    }
    if (suite_name == "black_runner") {
        run_black_runner_tests();
        return;
    }
    if (suite_name == "ganak_runner") {
        run_ganak_runner_tests();
        return;
    }
    if (suite_name == "spot_runner") {
        run_spot_runner_tests();
        return;
    }
    if (suite_name == "crossover") {
        run_crossover_tests();
        return;
    }
    if (suite_name == "generation") {
        run_generation_tests();
        return;
    }
    if (suite_name == "mutation") {
        run_mutation_tests();
        return;
    }
    if (suite_name == "prop_formula_ast") {
        run_prop_formula_ast_tests();
        return;
    }
    if (suite_name == "prop_formula_cnf") {
        run_prop_formula_cnf_tests();
        return;
    }
    if (suite_name == "prop_formula_rewrite") {
        run_prop_formula_rewrite_tests();
        return;
    }
    if (suite_name == "prop_formula_similarity") {
        run_prop_formula_similarity_tests();
        return;
    }
    if (suite_name == "semantic_similarity") {
        run_semantic_similarity_tests();
        return;
    }
    if (suite_name == "syntactic_similarity") {
        run_syntactic_similarity_tests();
        return;
    }
    throw std::invalid_argument("Unknown test suite: " +
                                std::string(suite_name));
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 1) {
            run_transfer_matrix_tests();
            run_black_runner_tests();
            run_ganak_runner_tests();
            run_spot_runner_tests();
            run_crossover_tests();
            run_generation_tests();
            run_mutation_tests();
            run_prop_formula_ast_tests();
            run_prop_formula_cnf_tests();
            run_prop_formula_rewrite_tests();
            run_prop_formula_similarity_tests();
            run_semantic_similarity_tests();
            run_syntactic_similarity_tests();
            return 0;
        }
        if (argc != 2) {
            throw std::invalid_argument(
                "Expected zero arguments or exactly one test suite name.");
        }
        run_suite(argv[1]);
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
