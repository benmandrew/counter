cmake_minimum_required(VERSION 3.21)

project(counter VERSION 0.1.0 LANGUAGES CXX)

enable_testing()

include(CheckCXXSourceCompiles)

include(cmake/ganak.cmake)
include(cmake/dependencies.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

check_cxx_source_compiles(
    "int main() { unsigned __int128 value = 0; (void)value; return 0; }"
    COUNTER_HAS_UINT128)

add_library(counter_core STATIC)
target_sources(counter_core PRIVATE
    src/prop_formula.cpp
    src/requirement.cpp
)
target_include_directories(counter_core PUBLIC include)

add_library(counter_fitness STATIC)
target_sources(counter_fitness PRIVATE
    src/fitness/ganak_runner.cpp
    src/fitness/model_counter.cpp
    src/fitness/semantic_similarity.cpp
    src/fitness/syntactic_similarity.cpp
    src/fitness/transfer_matrix.cpp
)
target_include_directories(counter_fitness PUBLIC include)
target_link_libraries(counter_fitness PUBLIC counter_core Eigen3::Eigen)
target_compile_definitions(counter_fitness PRIVATE GANAK_EXECUTABLE_PATH="${GANAK_EXECUTABLE}")
target_compile_definitions(counter_fitness
    PUBLIC
    $<$<BOOL:${COUNTER_HAS_UINT128}>:COUNTER_USE_UINT128=1>
)

add_executable(counter src/main.cpp)
target_link_libraries(counter PRIVATE counter_fitness)

add_custom_target(ganak_binary ALL DEPENDS "${GANAK_EXECUTABLE}")

add_subdirectory(test)

install(PROGRAMS "${GANAK_EXECUTABLE}" DESTINATION bin RENAME ganak)

include(cmake/format.cmake)
include(cmake/lint.cmake)

foreach(counter_target counter_core counter_fitness counter)
    if(MSVC)
        target_compile_options(${counter_target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${counter_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
