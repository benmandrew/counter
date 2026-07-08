find_package(Doxygen OPTIONAL_COMPONENTS dot)
find_program(SPHINX_BUILD_EXE sphinx-build)

if(DOXYGEN_FOUND AND SPHINX_BUILD_EXE)
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/doxygen")
    set(DOXYGEN_XML_DIR "${DOXYGEN_OUTPUT_DIR}/xml")
    set(SPHINX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/sphinx")
    # Build the internal reference into a persistent directory (NOT under the
    # Sphinx output, which is wiped every build) so Doxygen's per-graph .md5
    # cache survives across runs and unchanged dot graphs are not re-rendered.
    # It is copied into the published site (internal/) after Sphinx runs.
    set(DOXYGEN_INTERNAL_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/internal")
    set(SPHINX_INTERNAL_DIR "${SPHINX_OUTPUT_DIR}/internal")

    if(DOXYGEN_DOT_FOUND)
        set(DOXYGEN_HAVE_DOT "YES")
    else()
        set(DOXYGEN_HAVE_DOT "NO")
    endif()

    configure_file(
        "${CMAKE_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_BINARY_DIR}/docs/Doxyfile"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/docs/Doxyfile.internal.in"
        "${CMAKE_BINARY_DIR}/docs/Doxyfile.internal"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/docs/conf.py.in"
        "${CMAKE_BINARY_DIR}/docs/conf.py"
        @ONLY
    )

    # The internal HTML is built into its own persistent dir (graph cache
    # survives across runs) and copied into the Sphinx output last, since
    # Sphinx wipes SPHINX_OUTPUT_DIR before building.
    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DOXYGEN_OUTPUT_DIR}"
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/docs/Doxyfile"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DOXYGEN_INTERNAL_OUTPUT_DIR}"
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/docs/Doxyfile.internal"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${SPHINX_OUTPUT_DIR}"
        COMMAND ${SPHINX_BUILD_EXE} -q -b html
            -c "${CMAKE_BINARY_DIR}/docs"
            "${CMAKE_SOURCE_DIR}/docs"
            "${SPHINX_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DOXYGEN_INTERNAL_OUTPUT_DIR}" "${SPHINX_INTERNAL_DIR}"
        COMMENT "Generating documentation with Doxygen and Sphinx"
        VERBATIM
    )
else()
    set(_docs_missing "")
    if(NOT DOXYGEN_FOUND)
        list(APPEND _docs_missing "doxygen")
    endif()
    if(NOT SPHINX_BUILD_EXE)
        list(APPEND _docs_missing "sphinx-build")
    endif()
    list(JOIN _docs_missing ", " _docs_missing_str)

    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E echo
            "docs requires: ${_docs_missing_str}"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Documentation generation requires doxygen and sphinx-build"
        VERBATIM
    )
endif()
