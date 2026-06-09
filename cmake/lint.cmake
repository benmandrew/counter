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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

set(COUNTER_LINT_CPP_GLOBS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
)

file(GLOB_RECURSE COUNTER_LINT_FILES CONFIGURE_DEPENDS ${COUNTER_LINT_GLOBS})
file(GLOB_RECURSE COUNTER_LINT_CPP_FILES CONFIGURE_DEPENDS ${COUNTER_LINT_CPP_GLOBS})

# --- cpplint ---

if(CPPLINT_EXE)
    add_custom_target(lint-cpplint
        COMMAND ${CPPLINT_EXE} --quiet ${COUNTER_LINT_FILES}
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

if(RUN_CLANG_TIDY_EXE)
    add_custom_target(lint-clang-tidy
        COMMAND ${RUN_CLANG_TIDY_EXE}
            -quiet
            -p ${CMAKE_BINARY_DIR}
            "^${CMAKE_CURRENT_SOURCE_DIR}/(src|test)/.*\\.cpp$"
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
            --suppress=missingIncludeSystem
            --suppress=unusedStructMember
            --suppress=useStlAlgorithm
            --suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/cppcheck_suppressions.txt
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
