cmake_minimum_required(VERSION 3.21)

project(counter VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

include(CheckCXXSourceCompiles)

if(NOT DEFINED CMAKE_BUILD_PARALLEL_LEVEL)
  include(ProcessorCount)
  ProcessorCount(NPROC)
  if(NPROC GREATER 1)
    set(CMAKE_BUILD_PARALLEL_LEVEL ${NPROC})
  else()
    set(CMAKE_BUILD_PARALLEL_LEVEL 1)
  endif()
endif()

include(cmake/ganak.cmake)
include(cmake/spot.cmake)
include(cmake/black.cmake)
include(cmake/dependencies.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(COUNTER_TSAN "Build with ThreadSanitizer" OFF)
if(COUNTER_TSAN)
    set(CMAKE_CROSSCOMPILING_EMULATOR
        "setarch" "${CMAKE_SYSTEM_PROCESSOR}" "-R"
        CACHE STRING "Disable ASLR for TSan shadow memory" FORCE)
    message(STATUS "TSan: wrapping tests with setarch ${CMAKE_SYSTEM_PROCESSOR} -R (ASLR disabled)")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

check_cxx_source_compiles(
    "int main() { unsigned __int128 value = 0; (void)value; return 0; }"
    COUNTER_HAS_UINT128)

add_library(counter_core STATIC)
target_sources(counter_core PRIVATE
    src/prop_formula/cnf.cpp
    src/prop_formula/core.cpp
    src/prop_formula/dimacs_api.cpp
    src/prop_formula/parser.cpp
    src/prop_formula/similarity.cpp
    src/prop_formula/transform.cpp
    src/requirement.cpp
    src/tlsf/parser.cpp
    src/tlsf/specification.cpp
    src/tlsf/writer.cpp
)
target_include_directories(counter_core PUBLIC include)
target_link_libraries(counter_core PUBLIC nlohmann_json::nlohmann_json)
# Include nlohmann as a system directory so clang-tidy skips warnings inside it.
target_include_directories(counter_core SYSTEM PUBLIC ${nlohmann_json_SOURCE_DIR}/include)

add_library(counter_fitness STATIC)
target_sources(counter_fitness PRIVATE
    src/fitness/function.cpp
    src/runner/black.cpp
    src/runner/formaliser.cpp
    src/runner/ganak.cpp
    src/runner/ltlfilt.cpp
    src/runner/spot.cpp
    src/thread_pool.cpp
    src/filter/bloat.cpp
    src/filter/implication.cpp
    src/filter/implication_check.cpp
    src/fitness/halstead.cpp
    src/fitness/model_counter.cpp
    src/fitness/semantic_similarity.cpp
    src/fitness/status.cpp
    src/fitness/syntactic_similarity.cpp
    src/fitness/transfer_matrix.cpp
    src/genetic/crossover.cpp
    src/genetic/generation.cpp
    src/genetic/mutation.cpp
    src/tlsf/fitness.cpp
    src/tlsf/mutation.cpp
    src/tlsf/crossover.cpp
    src/tlsf/operators.cpp
    src/tlsf/filter.cpp
    src/config_io.cpp
)
target_include_directories(counter_fitness PUBLIC include)
target_link_libraries(counter_fitness PUBLIC counter_core tomlplusplus::tomlplusplus)
add_dependencies(counter_fitness spot_project)
# Include Eigen as a system directory so clang-tidy skips warnings inside it.
target_include_directories(counter_fitness SYSTEM PUBLIC ${eigen_SOURCE_DIR})
target_compile_definitions(counter_fitness PRIVATE
    GANAK_EXECUTABLE_PATH="${GANAK_EXECUTABLE}"
    SPOT_BIN_DIR="${SPOT_BIN_DIR}"
    BLACK_EXECUTABLE_PATH="${BLACK_EXECUTABLE}"
)
target_compile_definitions(counter_fitness
    PUBLIC
    $<$<BOOL:${COUNTER_HAS_UINT128}>:COUNTER_USE_UINT128=1>
)

if(DEFINED COUNTER_DEFAULT_FITNESS_WEIGHT)
    target_compile_definitions(counter_fitness PUBLIC
        COUNTER_DEFAULT_FITNESS_WEIGHT=${COUNTER_DEFAULT_FITNESS_WEIGHT})
endif()

# Defaults to the vendored copy (vendor/fretCLI.main.js, see vendor/README.md
# for provenance); override with
# `cmake -DFORMALISER_SCRIPT_PATH=/path/to/fretCLI.main.js` to point at a
# different copy. Still fails configure rather than deferring to the
# assert()/hang at runtime that a silently missing path causes (assert() is
# a no-op in NDEBUG builds, so the release and relwithdebinfo presets would
# otherwise spawn `node` with an empty script argument and hang indefinitely
# instead of failing fast) — this only fires now if the vendored file itself
# goes missing or an override is explicitly cleared.
if(NOT DEFINED FORMALISER_SCRIPT_PATH)
    set(FORMALISER_SCRIPT_PATH "${CMAKE_SOURCE_DIR}/vendor/fretCLI.main.js"
        CACHE FILEPATH "Path to fretCLI.main.js (formalize --batch)")
endif()
if(NOT FORMALISER_SCRIPT_PATH)
    message(FATAL_ERROR
        "FORMALISER_SCRIPT_PATH is not set. Configure with "
        "-DFORMALISER_SCRIPT_PATH=/path/to/fretCLI.main.js")
endif()
target_compile_definitions(counter_fitness PRIVATE
    FORMALISER_SCRIPT_PATH="${FORMALISER_SCRIPT_PATH}")

add_executable(counter src/main.cpp src/crash/crash_handler.cpp)
target_link_libraries(counter PRIVATE counter_fitness cpptrace::cpptrace)

add_executable(signal_tracer src/crash/signal_tracer.cpp)
target_link_libraries(signal_tracer PRIVATE cpptrace::cpptrace)

add_executable(compare src/compare.cpp)
target_link_libraries(compare PRIVATE counter_fitness)

add_executable(realize src/realize.cpp)
target_link_libraries(realize PRIVATE counter_fitness)

add_executable(ltl src/ltl.cpp)
target_link_libraries(ltl PRIVATE counter_fitness)

add_custom_target(ganak_binary ALL DEPENDS "${GANAK_EXECUTABLE}")
add_custom_target(black_binary ALL DEPENDS "${BLACK_EXECUTABLE}")
if(DEFINED BLACK_EXTERNAL_TARGET)
    add_dependencies(black_binary ${BLACK_EXTERNAL_TARGET})
endif()

add_subdirectory(test)

option(COUNTER_BENCH "Build and register benchmark tests" OFF)
if(COUNTER_BENCH)
    add_subdirectory(bench)
endif()

option(COUNTER_FUZZ
    "Build the LTL-equivalence libFuzzer target (requires a clang++ with libFuzzer support)"
    OFF)
if(COUNTER_FUZZ)
    add_subdirectory(fuzz)
endif()

install(PROGRAMS "${GANAK_EXECUTABLE}" DESTINATION bin RENAME ganak)

include(cmake/docs.cmake)
include(cmake/format.cmake)
include(cmake/lint.cmake)

foreach(counter_target counter_core counter_fitness counter compare realize ltl)
    if(MSVC)
        target_compile_options(${counter_target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${counter_target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

