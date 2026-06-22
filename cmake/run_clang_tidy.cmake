# Wrapper invoked by the lint-clang-tidy target. Runs run-clang-tidy and
# strips the two noisy line types from its output before printing:
#   - "<clang-tidy binary> ... file.cpp" invocation echoes (run-clang-tidy
#     writes this unconditionally per file, regardless of -quiet -- that
#     flag only suppresses clang-tidy's own "N warnings generated" output)
#   - "N warnings generated." per-TU diagnostic summaries
set(CLANG_TIDY_BINARY_ARGS "")
if(CLANG_TIDY_EXE)
    # run-clang-tidy wrapper scripts default to a version-suffixed binary
    # name baked into the script itself (e.g. clang-tidy-14); pin it
    # explicitly so it can't silently diverge from the clang-tidy CMake
    # resolved on PATH.
    set(CLANG_TIDY_BINARY_ARGS "-clang-tidy-binary=${CLANG_TIDY_EXE}")
endif()
execute_process(
    COMMAND ${RUN_CLANG_TIDY_EXE}
        -quiet
        -p ${BUILD_DIR}
        ${CLANG_TIDY_BINARY_ARGS}
        ${FILES_PATTERN}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    RESULT_VARIABLE result
)
# The invocation echo starts with the exact binary path we pinned above
# (CLANG_TIDY_EXE), not the unqualified "clang-tidy-<version>" name the
# stripped pattern used to assume before pinning was added.
if(CLANG_TIDY_EXE)
    string(REGEX REPLACE "${CLANG_TIDY_EXE}[^\n]*\n?" "" output "${output}")
endif()
string(REGEX REPLACE "[0-9]+ warnings generated\\.[^\n]*\n?" "" output "${output}")
string(STRIP "${output}" output)
string(REGEX REPLACE "[0-9]+ warnings generated\\.[^\n]*\n?" "" error_output "${error_output}")
string(STRIP "${error_output}" error_output)
if(output)
    message("${output}")
endif()
if(error_output)
    message("${error_output}")
endif()
if(result)
    message(FATAL_ERROR "clang-tidy failed (exit ${result})")
endif()
