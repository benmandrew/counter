find_program(CLANG_FORMAT_EXE NAMES clang-format)

set(COUNTER_FORMAT_GLOBS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

file(GLOB_RECURSE COUNTER_FORMAT_FILES CONFIGURE_DEPENDS ${COUNTER_FORMAT_GLOBS})

if(CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${COUNTER_FORMAT_FILES}
        COMMENT "Formatting C++ sources with clang-format"
        VERBATIM
    )
else()
    add_custom_target(format
        COMMAND ${CMAKE_COMMAND} -E echo "clang-format was not found on PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Formatting requires clang-format"
        VERBATIM
    )
endif()
