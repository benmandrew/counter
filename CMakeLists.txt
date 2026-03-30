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

set(COUNTER_LIBRARY_SOURCES
    src/formula_dimacs.cpp
    src/ganak_runner.cpp
    src/model_counter.cpp
    src/requirement.cpp
    src/transfer_matrix.cpp
    src/semantic_similarity.cpp
)

add_library(counter_static STATIC
    ${COUNTER_LIBRARY_SOURCES}
)

target_include_directories(counter_static PUBLIC include)
target_link_libraries(counter_static PUBLIC Eigen3::Eigen)
target_compile_definitions(counter_static PRIVATE GANAK_EXECUTABLE_PATH="${GANAK_EXECUTABLE}")
target_compile_definitions(counter_static
    PUBLIC
    $<$<BOOL:${COUNTER_HAS_UINT128}>:COUNTER_USE_UINT128=1>
)

add_executable(counter src/main.cpp)
target_link_libraries(counter PRIVATE counter_static)

add_custom_target(ganak_binary ALL DEPENDS "${GANAK_EXECUTABLE}")

add_subdirectory(test)

install(PROGRAMS "${GANAK_EXECUTABLE}" DESTINATION bin RENAME ganak)

include(cmake/format.cmake)
include(cmake/lint.cmake)

foreach(counter_target counter_static counter)
    if(MSVC)
        target_compile_options(${counter_target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${counter_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
