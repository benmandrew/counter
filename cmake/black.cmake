find_program(BLACK_EXECUTABLE black)

if(NOT BLACK_EXECUTABLE)
    set(BLACK_VERSION "25.09.0")
    set(BLACK_DOWNLOAD_URL
        "https://github.com/black-sat/black/releases/download/v${BLACK_VERSION}/black-sat-${BLACK_VERSION}.ubuntu24.04.x86_64.deb")
    set(BLACK_ROOT_DIR "${CMAKE_BINARY_DIR}/third_party/black")
    set(BLACK_ARCHIVE_PATH
        "${BLACK_ROOT_DIR}/black-sat-${BLACK_VERSION}.ubuntu24.04.x86_64.deb")
    set(BLACK_EXTRACT_DIR "${BLACK_ROOT_DIR}/extracted")
    set(BLACK_EXECUTABLE "${BLACK_EXTRACT_DIR}/usr/bin/black")

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

    if(NOT EXISTS "${BLACK_EXECUTABLE}")
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

    if(NOT EXISTS "${BLACK_EXECUTABLE}")
        message(FATAL_ERROR
            "black executable was not found after extraction: ${BLACK_EXECUTABLE}")
    endif()

    if(UNIX)
        file(CHMOD
            "${BLACK_EXECUTABLE}"
            PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
        )
    endif()
endif()

set(BLACK_EXECUTABLE "${BLACK_EXECUTABLE}" CACHE FILEPATH "Path to black executable")
message(STATUS "Using black executable: ${BLACK_EXECUTABLE}")
