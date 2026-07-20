# Wrapper invoked by the lint-clang-tidy target. Runs run-clang-tidy and
# strips the two noisy line types from its output before printing:
#   - "<clang-tidy binary> ... file.cpp" invocation echoes (run-clang-tidy
#     writes this unconditionally per file, regardless of -quiet -- that
#     flag only suppresses clang-tidy's own "N warnings generated" output)
#   - "N warnings generated." per-TU diagnostic summaries
# The binary run-clang-tidy invokes per translation unit. Normally the real
# clang-tidy, pinned explicitly (run-clang-tidy wrapper scripts otherwise
# default to a version-suffixed name baked into the script, e.g. clang-tidy-14,
# which can silently diverge from the clang-tidy CMake resolved on PATH).
set(CLANG_TIDY_INVOKED_BINARY "${CLANG_TIDY_EXE}")

# Opt-in ctcache interposition: only when a clang-tidy-cache binary is on PATH
# AND CTCACHE_DIR is set in the environment. In that case run-clang-tidy is
# pointed at a wrapper that routes each clang-tidy call through the cache, so
# unchanged translation units skip re-analysis. When either condition is
# absent, the invocation below is byte-for-byte the previous behavior (real
# clang-tidy, invoked directly), so local `nix develop` is unaffected.
set(RUN_CLANG_TIDY_LAUNCHER "")
find_program(CLANG_TIDY_CACHE_EXE NAMES clang-tidy-cache)
if(CLANG_TIDY_EXE AND CLANG_TIDY_CACHE_EXE AND DEFINED ENV{CTCACHE_DIR})
    set(CLANG_TIDY_INVOKED_BINARY "${CMAKE_CURRENT_LIST_DIR}/clang_tidy_cache_wrapper.sh")
    set(RUN_CLANG_TIDY_LAUNCHER ${CMAKE_COMMAND} -E env
        "CTCACHE_CACHE_BIN=${CLANG_TIDY_CACHE_EXE}"
        "CTCACHE_REAL_CLANG_TIDY=${CLANG_TIDY_EXE}"
        --)
endif()

set(CLANG_TIDY_BINARY_ARGS "")
if(CLANG_TIDY_INVOKED_BINARY)
    set(CLANG_TIDY_BINARY_ARGS "-clang-tidy-binary=${CLANG_TIDY_INVOKED_BINARY}")
endif()
execute_process(
    COMMAND ${RUN_CLANG_TIDY_LAUNCHER} ${RUN_CLANG_TIDY_EXE}
        -quiet
        -p ${BUILD_DIR}
        ${CLANG_TIDY_BINARY_ARGS}
        ${FILES_PATTERN}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error_output
    RESULT_VARIABLE result
)
# The invocation echo starts with the exact binary path we pinned above
# (the real clang-tidy, or the cache wrapper when interposing), not the
# unqualified "clang-tidy-<version>" name the stripped pattern used to assume
# before pinning was added.
if(CLANG_TIDY_INVOKED_BINARY)
    string(REGEX REPLACE "${CLANG_TIDY_INVOKED_BINARY}[^\n]*\n?" "" output "${output}")
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
