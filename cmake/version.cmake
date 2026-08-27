find_package(Git QUIET)

set(COUNTER_VERSION_HEADER_DIR "${CMAKE_BINARY_DIR}/generated")
set(COUNTER_VERSION_HEADER "${COUNTER_VERSION_HEADER_DIR}/git_version.hpp")
set(COUNTER_VERSION_TEMPLATE "${CMAKE_CURRENT_LIST_DIR}/version.hpp.in")
set(COUNTER_VERSION_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/write_version_header.cmake")

set(COUNTER_VERSION_ARGS
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
    "-DTEMPLATE_FILE=${COUNTER_VERSION_TEMPLATE}"
    "-DOUTPUT_FILE=${COUNTER_VERSION_HEADER}"
)

# -DCOUNTER_GIT_COMMIT=<sha> names the commit for a build whose source is a
# copy rather than a work tree, which is how the Docker image is built (see
# docs/docker.md). Without it such a build reports commit=unknown, and
# scripts/run_experiments.py refuses to launch a campaign against a binary that
# cannot say what it was built from. The full 40 characters are required: the
# abbreviation is what gets printed, not what gets recorded, and a short sha
# stops resolving as the repository grows.
if(COUNTER_GIT_COMMIT)
    # Length checked separately from the alphabet: cmake's regex engine has no
    # {n} repetition, and "^[0-9a-f]{40}$" quietly matches nothing at all rather
    # than failing to compile.
    string(LENGTH "${COUNTER_GIT_COMMIT}" _counter_sha_length)
    if(NOT _counter_sha_length EQUAL 40
       OR NOT COUNTER_GIT_COMMIT MATCHES "^[0-9a-f]+$")
        message(FATAL_ERROR
            "COUNTER_GIT_COMMIT must be a full 40-character lowercase hex sha, "
            "got '${COUNTER_GIT_COMMIT}'")
    endif()
    list(APPEND COUNTER_VERSION_ARGS "-DCOMMIT_OVERRIDE=${COUNTER_GIT_COMMIT}")
    if(COUNTER_GIT_COMMIT_SHORT)
        list(APPEND COUNTER_VERSION_ARGS
            "-DCOMMIT_SHORT_OVERRIDE=${COUNTER_GIT_COMMIT_SHORT}")
    endif()
    if(COUNTER_GIT_DIRTY)
        list(APPEND COUNTER_VERSION_ARGS "-DDIRTY_OVERRIDE=${COUNTER_GIT_DIRTY}")
    endif()
    message(STATUS "Version header pinned to ${COUNTER_GIT_COMMIT}")
elseif(COUNTER_GIT_COMMIT_SHORT OR COUNTER_GIT_DIRTY)
    message(FATAL_ERROR
        "COUNTER_GIT_COMMIT_SHORT and COUNTER_GIT_DIRTY only mean anything "
        "alongside -DCOUNTER_GIT_COMMIT=<sha>")
endif()

# Once at configure time so the header exists before the first compile (and for
# tools that read compile_commands.json without building), and again as a
# target that runs on every build so the recorded commit tracks HEAD rather
# than whenever cmake last ran. The script rewrites the file only when the
# rendered content differs, so the second pass is normally a no-op.
execute_process(
    COMMAND ${CMAKE_COMMAND} ${COUNTER_VERSION_ARGS} -P "${COUNTER_VERSION_SCRIPT}"
    RESULT_VARIABLE _counter_version_rc
)
if(NOT _counter_version_rc EQUAL 0)
    message(FATAL_ERROR "Failed to generate ${COUNTER_VERSION_HEADER}")
endif()

add_custom_target(counter_version_header
    COMMAND ${CMAKE_COMMAND} ${COUNTER_VERSION_ARGS} -P "${COUNTER_VERSION_SCRIPT}"
    BYPRODUCTS "${COUNTER_VERSION_HEADER}"
    COMMENT "Resolving the git commit for git_version.hpp"
    VERBATIM
)
