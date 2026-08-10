add_executable(counter_tests
    runner/black_tests.cpp
    runner/formaliser_tests.cpp
    runner/ganak_tests.cpp
    runner/ltlfilt_tests.cpp
    runner/process_tests.cpp
    runner/spot_tests.cpp
    filter/correctness_tests.cpp
    filter/implication_tests.cpp
    filter/vacuity_tests.cpp
    filter/well_separation_tests.cpp
    genetic/crossover_tests.cpp
    genetic/generation_tests.cpp
    genetic/determinism_tests.cpp
    genetic/pipeline_tests.cpp
    genetic/mutation_tests.cpp
    genetic/nsga2_tests.cpp
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
    dashboard_tests.cpp
    driver_support_tests.cpp
    profile_tests.cpp
    thread_pool_tests.cpp
    tlsf/parser_tests.cpp
    tlsf/writer_tests.cpp
    tlsf/filter_tests.cpp
    tlsf/fitness_tests.cpp
    tlsf/mucs_tests.cpp
    tlsf/genetic_tests.cpp
    tlsf/pipeline_tests.cpp
)

# src/ as well as test/, so suites can reach the private headers next to the
# implementation they cover (driver_support.hpp and the like). Those are
# deliberately not in include/, which is the published API surface.
target_include_directories(counter_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(counter_tests PRIVATE counter_fitness)

if(MSVC)
    target_compile_options(counter_tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(counter_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME counter_tests.profile COMMAND counter_tests profile)
# Again with the profiler switched on. The suite branches on
# profile::enabled(), so registering it only once means every assertion about
# actually recording anything is skipped -- the suite would pass by testing that
# a disabled profiler does nothing, which is the half that cannot regress.
add_test(NAME counter_tests.profile_enabled COMMAND counter_tests profile)
set_tests_properties(counter_tests.profile_enabled PROPERTIES
    ENVIRONMENT "COUNTER_PROFILE=1")
add_test(NAME counter_tests.thread_pool_default COMMAND counter_tests thread_pool)
# And again with a size requested. The global pool is built once per process and
# never resized, so the default and the requested width cannot both be observed
# in one run -- each case needs a process of its own, which is what two
# registrations buy. For the same reason the suite is not in the no-argument
# run-everything path in main.cpp.
add_test(NAME counter_tests.thread_pool_sized COMMAND counter_tests thread_pool)
set_tests_properties(counter_tests.thread_pool_sized PROPERTIES
    ENVIRONMENT "COUNTER_TEST_POOL_SIZE=3")
add_test(NAME counter_tests.transfer_matrix COMMAND counter_tests transfer_matrix)
add_test(NAME counter_tests.black_runner COMMAND counter_tests black_runner)
add_test(NAME counter_tests.formaliser_runner COMMAND counter_tests formaliser_runner)
add_test(NAME counter_tests.ganak_runner COMMAND counter_tests ganak_runner)
add_test(NAME counter_tests.ltlfilt_runner COMMAND counter_tests ltlfilt_runner)
add_test(NAME counter_tests.process_runner COMMAND counter_tests process_runner)
add_test(NAME counter_tests.spot_runner COMMAND counter_tests spot_runner)
add_test(NAME counter_tests.crossover COMMAND counter_tests crossover)
add_test(NAME counter_tests.generation COMMAND counter_tests generation)
add_test(NAME counter_tests.determinism COMMAND counter_tests determinism)
add_test(NAME counter_tests.pipeline COMMAND counter_tests pipeline)
add_test(NAME counter_tests.nsga2 COMMAND counter_tests nsga2)
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
add_test(NAME counter_tests.correctness COMMAND counter_tests correctness)
add_test(NAME counter_tests.implication_filter COMMAND counter_tests implication_filter)
add_test(NAME counter_tests.vacuity_filter COMMAND counter_tests vacuity_filter)
add_test(NAME counter_tests.well_separation_filter COMMAND counter_tests well_separation_filter)
add_test(NAME counter_tests.requirement COMMAND counter_tests requirement)
add_test(NAME counter_tests.serialisation COMMAND counter_tests serialisation)
add_test(NAME counter_tests.config_io COMMAND counter_tests config_io)
add_test(NAME counter_tests.dashboard COMMAND counter_tests dashboard)
add_test(NAME counter_tests.driver_support COMMAND counter_tests driver_support)
add_test(NAME counter_tests.tlsf_parser COMMAND counter_tests tlsf_parser)
add_test(NAME counter_tests.tlsf_writer COMMAND counter_tests tlsf_writer)
add_test(NAME counter_tests.tlsf_filter COMMAND counter_tests tlsf_filter)
add_test(NAME counter_tests.tlsf_fitness COMMAND counter_tests tlsf_fitness)
add_test(NAME counter_tests.tlsf_mucs COMMAND counter_tests tlsf_mucs)
add_test(NAME counter_tests.tlsf_genetic COMMAND counter_tests tlsf_genetic)
add_test(NAME counter_tests.tlsf_pipeline COMMAND counter_tests tlsf_pipeline)

# The dashboard page ships as one self-contained HTML file, so its script is
# tested by extracting it from the page and running it under node -- the same
# node the FRET formaliser already needs at run time. It is not a build
# dependency, so a missing one skips these rather than failing configuration.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    file(GLOB dashboard_page_tests CONFIGURE_DEPENDS
         ${CMAKE_CURRENT_SOURCE_DIR}/web/*.test.mjs)
    add_test(NAME dashboard_page
             COMMAND ${NODE_EXECUTABLE} --test ${dashboard_page_tests})
else()
    message(STATUS "node not found; skipping the dashboard page's tests")
endif()

add_custom_target(tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS counter_tests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and running all tests"
    VERBATIM
)
