#pragma once

#include <chrono>

void run_transfer_matrix_tests();
void run_black_runner_tests(const std::chrono::milliseconds& timeout);
void run_formaliser_runner_tests();
void run_ganak_runner_tests();
void run_ltlfilt_runner_tests();
void run_spot_runner_tests();
void run_crossover_tests();
void run_generation_tests();
void run_mutation_tests();
void run_prop_formula_ast_tests();
void run_prop_formula_cnf_tests();
void run_prop_formula_rewrite_tests();
void run_prop_formula_similarity_tests();
void run_prop_formula_temporal_tests();
void run_halstead_tests();
void run_semantic_similarity_tests();
void run_syntactic_similarity_tests();
void run_fitness_function_tests();
void run_status_tests();
void run_implication_filter_tests();
void run_requirement_tests();
void run_serialisation_tests();
void run_config_io_tests();
void run_tlsf_parser_tests();
void run_tlsf_writer_tests();
void run_tlsf_fitness_tests();
void run_tlsf_genetic_tests();
void run_tlsf_pipeline_tests();
