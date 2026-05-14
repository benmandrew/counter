include(FetchContent)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
)

# Eigen's blas/ subdir unconditionally calls check_language(Fortran).
# On macOS, gfortran hangs during the link test. Pre-setting this cache
# variable to empty makes check_language return "not found" immediately
# without running any compiler test, so enable_language(Fortran) is skipped.
set(CMAKE_Fortran_COMPILER "" CACHE FILEPATH "" FORCE)
FetchContent_MakeAvailable(eigen)
unset(CMAKE_Fortran_COMPILER CACHE)
