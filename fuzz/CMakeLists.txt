# The libFuzzer runtime (-fsanitize=fuzzer) is a clang/compiler-rt feature
# with no GCC equivalent, so this target is built with its own clang++
# invocation via add_custom_command rather than add_executable — the rest of
# the project keeps building with whichever compiler the active preset uses
# (GCC in the debug preset). Linking a clang+libFuzzer-compiled object
# against the GCC-compiled, ASAN/UBSAN-instrumented counter_core/
# counter_fitness static libraries works in practice (verified empirically)
# without needing a second, fully clang-built copy of the whole dependency
# tree (which would re-trigger SPOT's from-source ExternalProject build).
find_program(COUNTER_FUZZ_CLANGXX NAMES clang++)
if(NOT COUNTER_FUZZ_CLANGXX)
    message(FATAL_ERROR
        "COUNTER_FUZZ requires a clang++ with libFuzzer support on PATH; "
        "none found. Install one (e.g. a system clang package) or disable "
        "with -DCOUNTER_FUZZ=OFF.")
endif()

set(COUNTER_FUZZ_BINARY "${CMAKE_CURRENT_BINARY_DIR}/ltl_equivalence_fuzzer")
set(COUNTER_FUZZ_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/ltl_equivalence_fuzzer.cpp")

add_custom_command(
    OUTPUT "${COUNTER_FUZZ_BINARY}"
    COMMAND "${COUNTER_FUZZ_CLANGXX}"
            -std=c++17 -O1 -g
            -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all
            "-I${CMAKE_SOURCE_DIR}/include"
            "${COUNTER_FUZZ_SOURCE}"
            "$<TARGET_FILE:counter_fitness>"
            "$<TARGET_FILE:counter_core>"
            -o "${COUNTER_FUZZ_BINARY}"
    DEPENDS "${COUNTER_FUZZ_SOURCE}" counter_fitness counter_core
    COMMENT "Building ltl_equivalence_fuzzer with ${COUNTER_FUZZ_CLANGXX} (libFuzzer)"
    VERBATIM
)
add_custom_target(ltl_equivalence_fuzzer ALL DEPENDS "${COUNTER_FUZZ_BINARY}")
