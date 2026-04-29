find_package(Doxygen)
find_program(SPHINX_BUILD_EXE sphinx-build)

if(DOXYGEN_FOUND AND SPHINX_BUILD_EXE)
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/doxygen")
    set(DOXYGEN_XML_DIR "${DOXYGEN_OUTPUT_DIR}/xml")
    set(SPHINX_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/sphinx")

    configure_file(
        "${CMAKE_SOURCE_DIR}/docs/Doxyfile.in"
        "${CMAKE_BINARY_DIR}/docs/Doxyfile"
        @ONLY
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/docs/conf.py.in"
        "${CMAKE_BINARY_DIR}/docs/conf.py"
        @ONLY
    )

    add_custom_target(docs
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DOXYGEN_OUTPUT_DIR}"
        COMMAND ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/docs/Doxyfile"
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${SPHINX_OUTPUT_DIR}"
        COMMAND ${SPHINX_BUILD_EXE} -q -b html
            -c "${CMAKE_BINARY_DIR}/docs"
            "${CMAKE_SOURCE_DIR}/docs"
            "${SPHINX_OUTPUT_DIR}"
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
