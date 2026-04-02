add_executable(counter_tests
    fitness/ganak_runner_tests.cpp
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

add_test(NAME counter_tests COMMAND counter_tests)

add_custom_target(tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS counter_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running all tests"
    VERBATIM
)
