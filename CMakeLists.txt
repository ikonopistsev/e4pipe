cmake_minimum_required(VERSION 3.10)

project(e4pipe VERSION 1.8.0 LANGUAGES C)

option(E4PIPE_LIBRARY_STATIC "Set library type to STATIC" ON)
option(E4PIPE_BUILD_TESTS "Build tests" OFF)

set(CMAKE_C_STANDARD 11)

add_definitions("-DE4PIPE_VERSION=${PROJECT_VERSION}")

if (WIN32)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

set(SRC
    src/infinityseg.c
    src/infinitybuf.c
    src/infinitypipe.c
    src/pipeevent.c
    src/pipeevent-int.c
)

set(PUB_HEADER
    include/e4pipe/infinitypipe.h
    include/e4pipe/pipeevent.h
)

if (E4PIPE_LIBRARY_STATIC)
    add_library(e4pipe STATIC ${SRC})
    target_compile_definitions(e4pipe PUBLIC E4PIPE_STATIC)
else()
    add_library(e4pipe SHARED ${SRC})
    if (WIN32)
        target_compile_definitions(e4pipe PRIVATE E4PIPE_MAKEDLL)
    endif()
    set_target_properties(e4pipe PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)
endif()

set_target_properties(e4pipe
    PROPERTIES POSITION_INDEPENDENT_CODE
    ON
)

target_include_directories(e4pipe PUBLIC include/)
