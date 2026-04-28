add_executable(counter_tests
    fitness/black_runner_tests.cpp
    fitness/ganak_runner_tests.cpp
    fitness/spot_runner_tests.cpp
    genetic/crossover_tests.cpp
    genetic/generation_tests.cpp
    genetic/mutation_tests.cpp
    main.cpp
    fitness/semantic_similarity_tests.cpp
    fitness/syntactic_similarity_tests.cpp
    fitness/transfer_matrix_tests.cpp
    prop_formula/ast_tests.cpp
    prop_formula/cnf_tests.cpp
    prop_formula/rewrite_tests.cpp
    prop_formula/similarity_tests.cpp
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
add_test(NAME counter_tests.ganak_runner COMMAND counter_tests ganak_runner)
add_test(NAME counter_tests.spot_runner COMMAND counter_tests spot_runner)
add_test(NAME counter_tests.crossover COMMAND counter_tests crossover)
add_test(NAME counter_tests.generation COMMAND counter_tests generation)
add_test(NAME counter_tests.mutation COMMAND counter_tests mutation)
add_test(NAME counter_tests.prop_formula_ast COMMAND counter_tests prop_formula_ast)
add_test(NAME counter_tests.prop_formula_cnf COMMAND counter_tests prop_formula_cnf)
add_test(NAME counter_tests.prop_formula_rewrite COMMAND counter_tests prop_formula_rewrite)
add_test(NAME counter_tests.prop_formula_similarity COMMAND counter_tests prop_formula_similarity)
add_test(NAME counter_tests.semantic_similarity COMMAND counter_tests semantic_similarity)
add_test(NAME counter_tests.syntactic_similarity COMMAND counter_tests syntactic_similarity)

add_custom_target(tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS counter_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running all tests"
    VERBATIM
)
