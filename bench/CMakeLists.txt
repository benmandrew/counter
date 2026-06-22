include(FetchContent)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.9.1
    GIT_SHALLOW TRUE
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_DOWNLOAD_DEPENDENCIES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googlebenchmark)

add_executable(counter_bench
    bench_implication.cpp
    main.cpp
)

target_include_directories(counter_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(counter_bench PRIVATE counter_fitness benchmark::benchmark)

if(MSVC)
    target_compile_options(counter_bench PRIVATE /W4 /permissive-)
else()
    target_compile_options(counter_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Regression-detection test: asserts black-call counts are correct.
add_test(NAME counter_bench.assertions
         COMMAND counter_bench --check-assertions)
set_tests_properties(counter_bench.assertions PROPERTIES LABELS bench)

# Timing test: writes Google Benchmark JSON for CI perf tracking.
add_test(NAME counter_bench.timing
         COMMAND counter_bench
                 --benchmark_format=json
                 --benchmark_out=${CMAKE_CURRENT_BINARY_DIR}/bench_results.json
                 --benchmark_time_unit=ns)
set_tests_properties(counter_bench.timing PROPERTIES LABELS bench)
