# Resolves the git commit and writes it into the generated version header.
#
# Runs in script mode (-P) from a custom target, so it re-resolves on every
# build. Doing it at configure time instead would bake in whatever HEAD was
# when cmake last ran, and `cmake --build` after a new commit would keep
# reporting the old hash — which is the failure this whole mechanism exists to
# prevent.
#
# Expects GIT_EXECUTABLE, SOURCE_DIR, TEMPLATE_FILE and OUTPUT_FILE.

set(COUNTER_GIT_COMMIT "unknown")
set(COUNTER_GIT_COMMIT_SHORT "unknown")
set(COUNTER_GIT_DIRTY "false")

function(_counter_git out_var)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" ${ARGN}
        OUTPUT_VARIABLE _out
        RESULT_VARIABLE _rc
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT _rc EQUAL 0)
        set(_out "")
    endif()
    set(${out_var} "${_out}" PARENT_SCOPE)
endfunction()

if(GIT_EXECUTABLE AND EXISTS "${GIT_EXECUTABLE}")
    # A source tree exported from a tarball, or vendored into another
    # checkout, is not a work tree; fall back rather than fail the build.
    _counter_git(_inside rev-parse --is-inside-work-tree)
    if(_inside STREQUAL "true")
        _counter_git(_full rev-parse HEAD)
        _counter_git(_short rev-parse --short HEAD)
        if(_full)
            set(COUNTER_GIT_COMMIT "${_full}")
        endif()
        if(_short)
            set(COUNTER_GIT_COMMIT_SHORT "${_short}")
        endif()
        # --untracked-files=no on purpose: an untracked file is not compiled
        # into anything, so it cannot explain a binary that disagrees with its
        # commit. Modified tracked files can, and are what the flag reports.
        _counter_git(_status status --porcelain --untracked-files=no)
        if(NOT _status STREQUAL "")
            set(COUNTER_GIT_DIRTY "true")
        endif()
    endif()
endif()

# configure_file leaves the output untouched when the rendered content is
# identical, so a build that changed no commit does not invalidate every
# object file that transitively depends on the header.
configure_file("${TEMPLATE_FILE}" "${OUTPUT_FILE}" @ONLY)
