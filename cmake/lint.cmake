find_program(CPPLINT_EXE NAMES cpplint cpplint.py)

set(COUNTER_CPPLINT_GLOBS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/test/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

file(GLOB_RECURSE COUNTER_CPPLINT_FILES CONFIGURE_DEPENDS ${COUNTER_CPPLINT_GLOBS})

if(CPPLINT_EXE)
    add_custom_target(lint
        COMMAND ${CPPLINT_EXE} --quiet ${COUNTER_CPPLINT_FILES}
        COMMENT "Running cpplint on C++ sources"
        VERBATIM
    )
else()
    add_custom_target(lint
        COMMAND ${CMAKE_COMMAND} -E echo "cpplint was not found on PATH"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Linting requires cpplint"
        VERBATIM
    )
endif()
