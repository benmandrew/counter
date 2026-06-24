#include <chrono>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#include "config.hpp"
#include "runner/black.hpp"
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
    if (suite_name == "ltlfilt_runner") {
        run_ltlfilt_runner_tests();
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
    if (suite_name == "halstead") {
        run_halstead_tests();
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
    if (suite_name == "fitness_function") {
        run_fitness_function_tests();
        return;
    }
    if (suite_name == "status") {
        run_status_tests();
        return;
    }
    if (suite_name == "implication_filter") {
        run_implication_filter_tests();
        return;
    }
    if (suite_name == "requirement") {
        run_requirement_tests();
        return;
    }
    if (suite_name == "serialisation") {
        run_serialisation_tests();
        return;
    }
    if (suite_name == "config_io") {
        run_config_io_tests();
        return;
    }
    throw std::invalid_argument("Unknown test suite: " +
                                std::string(suite_name));
}

}  // namespace

int main(int argc, const char* const argv[]) {
    // Restore the larger pre-optimisation timeout for tests: the production
    // default in config.hpp is now tuned tight for real runs, but CI has
    // previously been slow enough to make that value flaky for tests that
    // expect a definite SAT/UNSAT answer rather than a timeout.
    Config cfg;
    cfg.black_timeout = std::chrono::milliseconds{10000};
    global_sat_checker().set_timeout(cfg.black_timeout);
    try {
        if (argc == 1) {
            run_transfer_matrix_tests();
            run_black_runner_tests();
            run_ganak_runner_tests();
            run_ltlfilt_runner_tests();
            run_spot_runner_tests();
            run_crossover_tests();
            run_generation_tests();
            run_mutation_tests();
            run_prop_formula_ast_tests();
            run_prop_formula_cnf_tests();
            run_prop_formula_rewrite_tests();
            run_prop_formula_similarity_tests();
            run_halstead_tests();
            run_semantic_similarity_tests();
            run_syntactic_similarity_tests();
            run_fitness_function_tests();
            run_status_tests();
            run_implication_filter_tests();
            run_requirement_tests();
            run_serialisation_tests();
            run_config_io_tests();
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
