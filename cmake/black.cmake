find_program(BLACK_EXECUTABLE black)

if(BLACK_EXECUTABLE)
    message(STATUS "Using black executable: ${BLACK_EXECUTABLE}")
    return()
endif()

set(BLACK_VERSION "25.09.0")
set(BLACK_ROOT_DIR "${CMAKE_BINARY_DIR}/third_party/black")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        message(FATAL_ERROR "No black-sat binary available for Linux ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(BLACK_DEB_NAME "black-sat-${BLACK_VERSION}.ubuntu24.04.x86_64.deb")
    set(BLACK_DOWNLOAD_URL
        "https://github.com/black-sat/black/releases/download/v${BLACK_VERSION}/${BLACK_DEB_NAME}")
    set(BLACK_ARCHIVE_PATH "${BLACK_ROOT_DIR}/${BLACK_DEB_NAME}")
    set(BLACK_EXTRACT_DIR "${BLACK_ROOT_DIR}/extracted")
    set(BLACK_BIN "${BLACK_EXTRACT_DIR}/usr/bin/black")
    set(BLACK_LIB_DIR "${BLACK_EXTRACT_DIR}/usr/lib")
    set(BLACK_WRAPPER "${BLACK_ROOT_DIR}/black")

    file(MAKE_DIRECTORY "${BLACK_ROOT_DIR}")

    if(NOT EXISTS "${BLACK_ARCHIVE_PATH}")
        message(STATUS "Downloading black-sat from ${BLACK_DOWNLOAD_URL}")
        file(DOWNLOAD
            "${BLACK_DOWNLOAD_URL}"
            "${BLACK_ARCHIVE_PATH}"
            STATUS black_download_status
            TLS_VERIFY ON
        )
        list(GET black_download_status 0 black_download_code)
        list(GET black_download_status 1 black_download_message)
        if(NOT black_download_code EQUAL 0)
            message(FATAL_ERROR "Failed to download black-sat: ${black_download_message}")
        endif()
    endif()

    if(NOT EXISTS "${BLACK_BIN}")
        file(MAKE_DIRECTORY "${BLACK_EXTRACT_DIR}")
        message(STATUS "Extracting black-sat package")
        execute_process(
            COMMAND dpkg-deb -x "${BLACK_ARCHIVE_PATH}" "${BLACK_EXTRACT_DIR}"
            RESULT_VARIABLE black_extract_result
        )
        if(NOT black_extract_result EQUAL 0)
            message(FATAL_ERROR "Failed to extract black-sat package.")
        endif()
    endif()

    if(NOT EXISTS "${BLACK_BIN}")
        message(FATAL_ERROR "black executable was not found after extraction: ${BLACK_BIN}")
    endif()

    file(WRITE "${BLACK_WRAPPER}"
        "#!/bin/sh\nexport LD_LIBRARY_PATH=\"${BLACK_LIB_DIR}:\${LD_LIBRARY_PATH}\"\nexec \"${BLACK_BIN}\" \"$@\"\n")
    file(CHMOD "${BLACK_WRAPPER}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )

    set(BLACK_EXECUTABLE "${BLACK_WRAPPER}" CACHE FILEPATH "Path to black executable" FORCE)

elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(ExternalProject)

    execute_process(
        COMMAND brew --prefix
        OUTPUT_VARIABLE HOMEBREW_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(NOT HOMEBREW_PREFIX)
        set(HOMEBREW_PREFIX "/opt/homebrew")
    endif()

    # tsl-hopscotch-map: header-only hash map, not in Homebrew
    set(TSL_INSTALL_DIR "${BLACK_ROOT_DIR}/deps/tsl-hopscotch")
    ExternalProject_Add(tsl_hopscotch_project
        GIT_REPOSITORY "https://github.com/Tessil/hopscotch-map.git"
        GIT_TAG "v2.4.0"
        GIT_SHALLOW TRUE
        PREFIX "${BLACK_ROOT_DIR}/deps/tsl-hopscotch-src"
        INSTALL_DIR "${TSL_INSTALL_DIR}"
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
    )

    # nlohmann_json: header-only JSON library, not in Homebrew
    set(NLOHMANN_INSTALL_DIR "${BLACK_ROOT_DIR}/deps/nlohmann")
    ExternalProject_Add(nlohmann_json_project
        GIT_REPOSITORY "https://github.com/nlohmann/json.git"
        GIT_TAG "v3.12.0"
        GIT_SHALLOW TRUE
        PREFIX "${BLACK_ROOT_DIR}/deps/nlohmann-src"
        INSTALL_DIR "${NLOHMANN_INSTALL_DIR}"
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DJSON_BuildTests=OFF
            -DJSON_Install=ON
        BUILD_COMMAND ""
        INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
    )

    set(BLACK_INSTALL_DIR "${BLACK_ROOT_DIR}/install")
    ExternalProject_Add(black_project
        URL "https://github.com/black-sat/black/archive/refs/tags/v${BLACK_VERSION}.tar.gz"
        TLS_VERIFY ON
        DOWNLOAD_EXTRACT_TIMESTAMP ON
        PREFIX "${BLACK_ROOT_DIR}/src"
        INSTALL_DIR "${BLACK_INSTALL_DIR}"
        LIST_SEPARATOR |
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
            -DCMAKE_PREFIX_PATH=${TSL_INSTALL_DIR}|${NLOHMANN_INSTALL_DIR}|${HOMEBREW_PREFIX}
            -DENABLE_TESTS=OFF
            -DENABLE_FORMULAS_TESTS=OFF
            -DGIT_UPDATE_SUBMODULES=OFF
            -DCMAKE_MACOSX_RPATH=ON
            -DCMAKE_CXX_FLAGS=-Wno-nrvo
        BUILD_COMMAND
            ${CMAKE_MAKE_PROGRAM} -j ${CMAKE_BUILD_PARALLEL_LEVEL} frontend
        INSTALL_COMMAND
            ${CMAKE_COMMAND} --install <BINARY_DIR>
        DEPENDS
            tsl_hopscotch_project
            nlohmann_json_project
    )

    set(BLACK_EXECUTABLE "${BLACK_INSTALL_DIR}/bin/black"
        CACHE FILEPATH "Path to black executable" FORCE)
    set(BLACK_EXTERNAL_TARGET "black_project")

else()
    message(FATAL_ERROR "No black-sat support for ${CMAKE_SYSTEM_NAME}")
endif()

message(STATUS "Using black executable: ${BLACK_EXECUTABLE}")
