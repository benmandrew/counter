set(COUNTER_TEST_SOURCES
    formula_dimacs_tests.cpp
    ganak_runner_tests.cpp
    main.cpp
    transfer_system_tests.cpp
    transfer_matrix_tests.cpp
)

add_executable(counter_tests
    ${COUNTER_TEST_SOURCES}
)

target_link_libraries(counter_tests PRIVATE counter_static)

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
