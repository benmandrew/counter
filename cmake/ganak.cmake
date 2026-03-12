set(GANAK_DOWNLOAD_URL
    "https://github.com/meelgroup/ganak/releases/download/release%2F2.5.3/ganak-linux-amd64.zip")
set(GANAK_ROOT_DIR "${CMAKE_BINARY_DIR}/third_party/ganak")
set(GANAK_ARCHIVE_PATH "${GANAK_ROOT_DIR}/ganak-linux-amd64.zip")
set(GANAK_EXTRACT_DIR "${GANAK_ROOT_DIR}/extracted")

file(MAKE_DIRECTORY "${GANAK_ROOT_DIR}")

if(NOT EXISTS "${GANAK_ARCHIVE_PATH}")
    message(STATUS "Downloading Ganak from ${GANAK_DOWNLOAD_URL}")
    file(DOWNLOAD
        "${GANAK_DOWNLOAD_URL}"
        "${GANAK_ARCHIVE_PATH}"
        STATUS ganak_download_status
        TLS_VERIFY ON
    )

    list(GET ganak_download_status 0 ganak_download_code)
    list(GET ganak_download_status 1 ganak_download_message)
    if(NOT ganak_download_code EQUAL 0)
        message(FATAL_ERROR "Failed to download Ganak: ${ganak_download_message}")
    endif()
endif()

if(NOT EXISTS "${GANAK_EXTRACT_DIR}")
    file(MAKE_DIRECTORY "${GANAK_EXTRACT_DIR}")
endif()

file(GLOB_RECURSE GANAK_CANDIDATES
    "${GANAK_EXTRACT_DIR}/ganak"
    "${GANAK_EXTRACT_DIR}/ganak-*"
    "${GANAK_EXTRACT_DIR}/*/ganak"
    "${GANAK_EXTRACT_DIR}/*/ganak-*"
)

if(NOT GANAK_CANDIDATES)
    message(STATUS "Extracting Ganak archive")
    file(ARCHIVE_EXTRACT
        INPUT "${GANAK_ARCHIVE_PATH}"
        DESTINATION "${GANAK_EXTRACT_DIR}"
    )

    file(GLOB_RECURSE GANAK_CANDIDATES
        "${GANAK_EXTRACT_DIR}/ganak"
        "${GANAK_EXTRACT_DIR}/ganak-*"
        "${GANAK_EXTRACT_DIR}/*/ganak"
        "${GANAK_EXTRACT_DIR}/*/ganak-*"
    )
endif()

if(NOT GANAK_CANDIDATES)
    message(FATAL_ERROR "Ganak executable was not found after extraction.")
endif()

list(GET GANAK_CANDIDATES 0 GANAK_EXECUTABLE)

if(UNIX)
    file(CHMOD
        "${GANAK_EXECUTABLE}"
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )
endif()

set(GANAK_EXECUTABLE "${GANAK_EXECUTABLE}" CACHE FILEPATH "Path to Ganak executable")
message(STATUS "Using Ganak executable: ${GANAK_EXECUTABLE}")
