include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG v0.7.5
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
)

# Must be set before FetchContent_MakeAvailable(cpptrace) so cpptrace's
# CMakeLists sees the value when it first runs.
set(CPPTRACE_UNWIND_WITH_LIBUNWIND ON CACHE BOOL "" FORCE)

# Eigen's blas/ subdir unconditionally calls check_language(Fortran).
# On macOS, gfortran hangs during the link test. Pre-setting this cache
# variable to empty makes check_language return "not found" immediately
# without running any compiler test, so enable_language(Fortran) is skipped.
set(CMAKE_Fortran_COMPILER "" CACHE FILEPATH "" FORCE)
FetchContent_MakeAvailable(eigen)
unset(CMAKE_Fortran_COMPILER CACHE)
FetchContent_MakeAvailable(cpptrace)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)
