include(ExternalProject)

set(SPOT_VERSION "2.15.1")
set(SPOT_DOWNLOAD_URL
    "https://www.lre.epita.fr/dload/spot/spot-${SPOT_VERSION}.tar.gz")
set(SPOT_INSTALL_DIR "${CMAKE_BINARY_DIR}/third_party/spot")
set(SPOT_BIN_DIR "${SPOT_INSTALL_DIR}/bin")

ExternalProject_Add(spot_project
    URL "${SPOT_DOWNLOAD_URL}"
    TLS_VERIFY ON
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    PREFIX "${CMAKE_BINARY_DIR}/third_party/spot_src"
    INSTALL_DIR "${SPOT_INSTALL_DIR}"
    CONFIGURE_COMMAND
        <SOURCE_DIR>/configure
        --prefix=<INSTALL_DIR>
        --disable-python
        --disable-doc
    BUILD_COMMAND
        ${CMAKE_MAKE_PROGRAM} -j ${CMAKE_BUILD_PARALLEL_LEVEL}
    INSTALL_COMMAND
        ${CMAKE_MAKE_PROGRAM} install
)

set(SPOT_BIN_DIR "${SPOT_BIN_DIR}" CACHE PATH "Path to Spot bin directory")
message(STATUS "Spot ${SPOT_VERSION} will be built into ${SPOT_INSTALL_DIR}")
