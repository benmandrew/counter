# Wrapper invoked by the lint-clang-tidy target. Runs run-clang-tidy and
# strips the two noisy line types from its output before printing:
#   - "clang-tidy-XX ... file.cpp" invocation echoes
#   - "N warnings generated." per-TU diagnostic summaries
execute_process(
    COMMAND ${RUN_CLANG_TIDY_EXE}
        -quiet
        -p ${BUILD_DIR}
        ${FILES_PATTERN}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    RESULT_VARIABLE result
)
string(REGEX REPLACE "clang-tidy-[^\n]*\n?" "" output "${output}")
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
