cmake_minimum_required(VERSION 3.21)

project(counter VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)
enable_testing()

find_program(CLANG_FORMAT_EXE NAMES clang-format)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(COUNTER_LIBRARY_SOURCES
    src/model_counter.cpp
    src/requirement.cpp
    src/transfer_matrix.cpp
)

set(COUNTER_APP_SOURCES
    src/main.cpp
)

set(COUNTER_FORMAT_GLOBS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

file(GLOB_RECURSE COUNTER_FORMAT_FILES CONFIGURE_DEPENDS ${COUNTER_FORMAT_GLOBS})

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(eigen)

add_library(counter_static STATIC
    ${COUNTER_LIBRARY_SOURCES}
)

target_include_directories(counter_static PUBLIC include)
target_link_libraries(counter_static PUBLIC Eigen3::Eigen)

add_executable(counter
    ${COUNTER_APP_SOURCES}
)

target_link_libraries(counter PRIVATE counter_static)

add_subdirectory(test)

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${COUNTER_FORMAT_FILES}
        COMMENT "Formatting C++ sources with clang-format"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found on PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Formatting requires clang-format"
        VERBATIM
    )
endif()

foreach(counter_target counter_static counter)
    if(MSVC)
        target_compile_options(${counter_target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${counter_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
