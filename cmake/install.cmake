# Staging rules for a self-contained installation.
#
# Everything counter spawns is resolved from an absolute path compiled into the
# binary (SPOT_BIN_DIR and friends, set in the root CMakeLists), and those paths
# point into the build tree. An installed tree therefore has to carry the
# solvers with it and say where they went, which is what the COUNTER_* env
# overrides read by src/runner/tool_paths.cpp are for. counter-env.sh writes
# those settings out beside the install; docs/docker.md bakes the same values
# into the image as ENV, where there is no shell to source from.
#
# The solvers land under libexec/ rather than bin/ deliberately. They are
# private to counter, and a host with its own ltlsynt or ganak on PATH must
# keep getting that one for its own use.
#
# Every rule here is in the `counter` component, so that
# `cmake --install <dir> --component counter` stages this project alone.
# FetchContent brings its dependencies in with add_subdirectory, which brings
# their install rules with them, and an unqualified install writes Eigen's
# headers and cpptrace's cmake config beside the binaries, neither of which
# belongs to this project.

include(GNUInstallDirs)

set(COUNTER_TOOL_DIR "${CMAKE_INSTALL_LIBEXECDIR}/counter")
set(COUNTER_DATA_DIR "${CMAKE_INSTALL_DATADIR}/counter")

option(COUNTER_INSTALL_TOOLS
    "Install the fetched solvers (Spot, black, Ganak) alongside the binaries"
    ON)

# See cmake/tool-wrapper.sh.in for why a wrapper is needed at all. The real
# binary lands one directory over from the name that gets executed, so the
# wrapper can take its place.
function(counter_install_wrapped_tool)
    cmake_parse_arguments(ARG "" "NAME;BINARY;DESTINATION" "" ${ARGN})

    set(COUNTER_WRAPPED_TOOL "${ARG_NAME}")
    set(_wrapper
        "${CMAKE_BINARY_DIR}/install-wrappers/${ARG_DESTINATION}/${ARG_NAME}")
    configure_file(
        "${CMAKE_CURRENT_LIST_DIR}/tool-wrapper.sh.in" "${_wrapper}" @ONLY)

    install(PROGRAMS "${_wrapper}"
        DESTINATION "${COUNTER_TOOL_DIR}/${ARG_DESTINATION}/bin"
        COMPONENT counter)
    install(PROGRAMS "${ARG_BINARY}"
        DESTINATION "${COUNTER_TOOL_DIR}/${ARG_DESTINATION}/libexec"
        RENAME "${ARG_NAME}"
        COMPONENT counter)
endfunction()

install(TARGETS
        counter
        compare
        lint-ideals
        ltl
        maximal
        mucs
        realize
        signal_tracer
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            COMPONENT counter)

# The vendored formaliser script and the dashboard page are read at run time
# from paths compiled in against the source tree, so an installed binary needs
# its own copy of both or the FRETISH path and `--dashboard` break the moment
# the sources are gone.
install(FILES "${FORMALISER_SCRIPT_PATH}"
    DESTINATION "${COUNTER_DATA_DIR}" COMPONENT counter)
install(FILES "${COUNTER_DASHBOARD_PAGE_PATH}"
    DESTINATION "${COUNTER_DATA_DIR}" COMPONENT counter)

# Small beside the binaries, and they are what the README quickstart and the
# TLSF guide both run. An image carrying no specification cannot demonstrate
# anything without the user supplying one first.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/examples"
    DESTINATION "${COUNTER_DATA_DIR}" COMPONENT counter)

if(COUNTER_INSTALL_TOOLS)
    # Three of Spot's sixteen binaries are ever spawned; the other thirteen are
    # tools nothing here calls. The static archives are excluded for the same
    # reason, nothing linking against them and libspot.a dwarfing the shared
    # library the image does load.
    foreach(_spot_tool ltlsynt ltl2tgba ltlfilt)
        counter_install_wrapped_tool(
            NAME "${_spot_tool}"
            BINARY "${SPOT_BIN_DIR}/${_spot_tool}"
            DESTINATION spot)
    endforeach()

    # install(DIRECTORY) preserves symlinks, which matters here: the NEEDED
    # entry is libspot.so.0 and only the symlink carries that name.
    install(DIRECTORY "${SPOT_INSTALL_DIR}/lib/"
        DESTINATION "${COUNTER_TOOL_DIR}/spot/lib"
        COMPONENT counter
        FILES_MATCHING
            PATTERN "libspot.so*"
            PATTERN "libbddx.so*"
            PATTERN "pkgconfig" EXCLUDE)

    # Statically linked, so it needs no wrapper.
    install(PROGRAMS "${GANAK_EXECUTABLE}"
        DESTINATION "${COUNTER_TOOL_DIR}" RENAME ganak COMPONENT counter)

    # BLACK_BIN and BLACK_LIB_DIR are left set by whichever branch of
    # cmake/black.cmake ran. Neither is set when black was found on PATH, since
    # that build never fetched one and the host's copy is not ours to install.
    if(BLACK_BIN)
        counter_install_wrapped_tool(
            NAME black
            BINARY "${BLACK_BIN}"
            DESTINATION black)
        install(DIRECTORY "${BLACK_LIB_DIR}/"
            DESTINATION "${COUNTER_TOOL_DIR}/black/lib"
            COMPONENT counter
            FILES_MATCHING PATTERN "*.so*")
    else()
        message(STATUS
            "black came from PATH; the install tree will not carry a copy")
    endif()
endif()

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/counter-env.sh.in"
    "${CMAKE_BINARY_DIR}/counter-env.sh"
    @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/counter-env.sh"
    DESTINATION "${COUNTER_DATA_DIR}" COMPONENT counter)
