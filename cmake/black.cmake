find_program(BLACK_EXECUTABLE black)

if(NOT BLACK_EXECUTABLE)
    set(BLACK_VERSION "25.09.0")
    set(BLACK_DOWNLOAD_URL
        "https://github.com/black-sat/black/releases/download/v${BLACK_VERSION}/black-sat-${BLACK_VERSION}.ubuntu24.04.x86_64.deb")
    set(BLACK_ROOT_DIR "${CMAKE_BINARY_DIR}/third_party/black")
    set(BLACK_ARCHIVE_PATH
        "${BLACK_ROOT_DIR}/black-sat-${BLACK_VERSION}.ubuntu24.04.x86_64.deb")
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
        message(FATAL_ERROR
            "black executable was not found after extraction: ${BLACK_BIN}")
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
endif()

message(STATUS "Using black executable: ${BLACK_EXECUTABLE}")
