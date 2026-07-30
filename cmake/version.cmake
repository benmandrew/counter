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
