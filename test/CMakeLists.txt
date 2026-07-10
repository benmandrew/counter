add_executable(counter_tests
    runner/black_tests.cpp
    runner/formaliser_tests.cpp
    runner/ganak_tests.cpp
    runner/ltlfilt_tests.cpp
    runner/spot_tests.cpp
    filter/implication_tests.cpp
    genetic/crossover_tests.cpp
    genetic/generation_tests.cpp
    genetic/mutation_tests.cpp
    main.cpp
    fitness/function_tests.cpp
    fitness/halstead_tests.cpp
    fitness/status_tests.cpp
    fitness/semantic_similarity_tests.cpp
    fitness/syntactic_similarity_tests.cpp
    fitness/transfer_matrix_tests.cpp
    requirement_tests.cpp
    prop_formula/ast_tests.cpp
    prop_formula/cnf_tests.cpp
    prop_formula/rewrite_tests.cpp
    prop_formula/similarity_tests.cpp
    prop_formula/temporal_tests.cpp
    serialisation_tests.cpp
    config_io_tests.cpp
)

target_include_directories(counter_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(counter_tests PRIVATE counter_fitness)

if(MSVC)
    target_compile_options(counter_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(counter_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME counter_tests.transfer_matrix COMMAND counter_tests transfer_matrix)
add_test(NAME counter_tests.black_runner COMMAND counter_tests black_runner)
add_test(NAME counter_tests.formaliser_runner COMMAND counter_tests formaliser_runner)
add_test(NAME counter_tests.ganak_runner COMMAND counter_tests ganak_runner)
add_test(NAME counter_tests.ltlfilt_runner COMMAND counter_tests ltlfilt_runner)
add_test(NAME counter_tests.spot_runner COMMAND counter_tests spot_runner)
add_test(NAME counter_tests.crossover COMMAND counter_tests crossover)
add_test(NAME counter_tests.generation COMMAND counter_tests generation)
add_test(NAME counter_tests.mutation COMMAND counter_tests mutation)
add_test(NAME counter_tests.prop_formula_ast COMMAND counter_tests prop_formula_ast)
add_test(NAME counter_tests.prop_formula_cnf COMMAND counter_tests prop_formula_cnf)
add_test(NAME counter_tests.prop_formula_rewrite COMMAND counter_tests prop_formula_rewrite)
add_test(NAME counter_tests.prop_formula_similarity COMMAND counter_tests prop_formula_similarity)
add_test(NAME counter_tests.prop_formula_temporal COMMAND counter_tests prop_formula_temporal)
add_test(NAME counter_tests.halstead COMMAND counter_tests halstead)
add_test(NAME counter_tests.semantic_similarity COMMAND counter_tests semantic_similarity)
add_test(NAME counter_tests.syntactic_similarity COMMAND counter_tests syntactic_similarity)
add_test(NAME counter_tests.fitness_function COMMAND counter_tests fitness_function)
add_test(NAME counter_tests.status COMMAND counter_tests status)
add_test(NAME counter_tests.implication_filter COMMAND counter_tests implication_filter)
add_test(NAME counter_tests.requirement COMMAND counter_tests requirement)
add_test(NAME counter_tests.serialisation COMMAND counter_tests serialisation)
add_test(NAME counter_tests.config_io COMMAND counter_tests config_io)

add_custom_target(tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS counter_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running all tests"
    VERBATIM
)
