find_program(CPPLINT_EXE NAMES cpplint cpplint.py)
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
find_program(RUN_CLANG_TIDY_EXE NAMES run-clang-tidy run-clang-tidy-14)
find_program(CPPCHECK_EXE NAMES cppcheck)

set(COUNTER_LINT_GLOBS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.h
    # fuzz/ is compiled via a raw clang++ add_custom_command (see
    # fuzz/CMakeLists.txt), not a normal CMake target, so it has no
    # compile_commands.json entry: only cpplint/cppcheck (compile-database
    # independent) run on it, not clang-tidy below.
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

set(COUNTER_LINT_CPP_GLOBS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bench/*.cpp
)

# Optional allow-list restricting linting to an explicit set of files, passed
# as an absolute-path list. CI sets this on pull requests to lint only the
# files a branch changed (the full sweep runs on pushes to main); local runs
# and main-branch CI leave it empty and lint everything. A header-only change
# yields an empty CPP subset, which the clang-tidy target below treats as a
# no-op rather than letting run-clang-tidy fall back to linting the whole
# compile database.
set(COUNTER_LINT_FILES_OVERRIDE "" CACHE STRING
    "Absolute paths of files to lint; empty means lint all sources")

if(COUNTER_LINT_FILES_OVERRIDE)
    set(COUNTER_LINT_FILES ${COUNTER_LINT_FILES_OVERRIDE})
    set(COUNTER_LINT_CPP_FILES "")
    foreach(lint_file IN LISTS COUNTER_LINT_FILES_OVERRIDE)
        if(lint_file MATCHES "^${CMAKE_CURRENT_SOURCE_DIR}/(src|test|bench)/.*\\.cpp$")
            list(APPEND COUNTER_LINT_CPP_FILES ${lint_file})
        endif()
    endforeach()
else()
    file(GLOB_RECURSE COUNTER_LINT_FILES CONFIGURE_DEPENDS ${COUNTER_LINT_GLOBS})
    file(GLOB_RECURSE COUNTER_LINT_CPP_FILES CONFIGURE_DEPENDS ${COUNTER_LINT_CPP_GLOBS})
endif()

# --- cpplint ---

if(CPPLINT_EXE)
    add_custom_target(lint-cpplint
        COMMAND ${CPPLINT_EXE}
            --config=.cpplint.cfg
            --quiet
            ${COUNTER_LINT_FILES}
        COMMENT "Running cpplint on C++ sources"
        VERBATIM
    )
else()
    add_custom_target(lint-cpplint
        COMMAND ${CMAKE_COMMAND} -E echo "WARNING: cpplint was not found on PATH, skipping"
        COMMENT "cpplint not available"
        VERBATIM
    )
endif()

# --- clang-tidy ---

# run-clang-tidy's positional args are source-file filter regexes matched
# against the compile database. With a changed-file allow-list, each .cpp path
# doubles as a regex matching exactly itself; otherwise use the tree-wide
# pattern.
if(COUNTER_LINT_FILES_OVERRIDE)
    set(COUNTER_CLANG_TIDY_FILTER ${COUNTER_LINT_CPP_FILES})
else()
    set(COUNTER_CLANG_TIDY_FILTER "^${CMAKE_CURRENT_SOURCE_DIR}/(src|test|bench)/.*\\.cpp$")
endif()

if(COUNTER_LINT_FILES_OVERRIDE AND NOT COUNTER_LINT_CPP_FILES)
    # Restricted to changed files, none of which are .cpp translation units:
    # skip rather than let run-clang-tidy fall back to the whole database.
    add_custom_target(lint-clang-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "clang-tidy: no changed .cpp files, skipping"
        COMMENT "clang-tidy: nothing to do"
        VERBATIM
    )
elseif(RUN_CLANG_TIDY_EXE)
    # run-clang-tidy wrapper scripts default -clang-tidy-binary to a
    # version-suffixed name (e.g. clang-tidy-14) baked into the script
    # itself, which silently diverges from whichever "clang-tidy" resolves
    # to on PATH if multiple LLVM versions are installed (older versions can
    # be missing checks that newer .clang-tidy configs rely on). Pin it
    # explicitly to the same binary CLANG_TIDY_EXE resolved to.
    add_custom_target(lint-clang-tidy
        COMMAND ${CMAKE_COMMAND}
            -DRUN_CLANG_TIDY_EXE=${RUN_CLANG_TIDY_EXE}
            -DCLANG_TIDY_EXE=${CLANG_TIDY_EXE}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            "-DFILES_PATTERN=${COUNTER_CLANG_TIDY_FILTER}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/run_clang_tidy.cmake
        COMMENT "Running clang-tidy on C++ sources"
        VERBATIM
    )
elseif(CLANG_TIDY_EXE)
    add_custom_target(lint-clang-tidy
        COMMAND ${CLANG_TIDY_EXE}
            --quiet
            -p ${CMAKE_BINARY_DIR}
            ${COUNTER_LINT_CPP_FILES}
        COMMENT "Running clang-tidy on C++ sources (single-threaded)"
        VERBATIM
    )
else()
    add_custom_target(lint-clang-tidy
        COMMAND ${CMAKE_COMMAND} -E echo "WARNING: clang-tidy was not found on PATH, skipping"
        COMMENT "clang-tidy not available"
        VERBATIM
    )
endif()

# --- cppcheck ---

if(CPPCHECK_EXE)
    add_custom_target(lint-cppcheck
        COMMAND ${CPPCHECK_EXE}
            --enable=warning,style,performance,portability
            --error-exitcode=1
            --suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppressions.txt
            --quiet
            ${COUNTER_LINT_FILES}
        COMMENT "Running cppcheck on C++ sources"
        VERBATIM
    )
else()
    add_custom_target(lint-cppcheck
        COMMAND ${CMAKE_COMMAND} -E echo "WARNING: cppcheck was not found on PATH, skipping"
        COMMENT "cppcheck not available"
        VERBATIM
    )
endif()

# --- aggregate ---

add_custom_target(lint)
add_dependencies(lint lint-cpplint lint-clang-tidy lint-cppcheck)
