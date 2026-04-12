cmake_minimum_required(VERSION 3.10)

project(e4pipe VERSION 1.8.0 LANGUAGES C)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

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
    include/e4pipe/infinitybuf.h
    include/e4pipe/infinitypipe.h
    include/e4pipe/infinitypipe_struct.h
    include/e4pipe/infinityseg.h
    include/e4pipe/pipeevent.h
    include/e4pipe/pipeevent_struct.h
)

set(E4PIPE_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/e4pipe")
set(E4PIPE_PACKAGE_MAINTAINER
    "e4pipe maintainers <packages@example.invalid>"
    CACHE STRING "Debian package maintainer")

function(e4pipe_configure_libevent_dependency)
    if (TARGET e4pipe_libevent_core)
        return()
    endif()

    add_library(e4pipe_libevent_core INTERFACE IMPORTED GLOBAL)

    if (TARGET libevent::core)
        set(_e4pipe_libevent_target libevent::core)
    elseif (TARGET event_core)
        set(_e4pipe_libevent_target event_core)
    else()
        find_package(Libevent CONFIG QUIET COMPONENTS core)
        if (TARGET libevent::core)
            set(_e4pipe_libevent_target libevent::core)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(E4PIPE_LIBEVENT_CORE REQUIRED IMPORTED_TARGET libevent_core)
            set(_e4pipe_libevent_target PkgConfig::E4PIPE_LIBEVENT_CORE)
        endif()
    endif()

    set_property(TARGET e4pipe_libevent_core PROPERTY
        INTERFACE_LINK_LIBRARIES "${_e4pipe_libevent_target}")
endfunction()

e4pipe_configure_libevent_dependency()

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

add_library(e4pipe::e4pipe ALIAS e4pipe)
add_library(E4Pipe::e4pipe ALIAS e4pipe)

set_target_properties(e4pipe PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(e4pipe
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(e4pipe PUBLIC e4pipe_libevent_core)

install(TARGETS e4pipe
    EXPORT e4pipeTargets
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT e4pipe_Runtime NAMELINK_COMPONENT e4pipe_Development
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT e4pipe_Development
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT e4pipe_Runtime
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(FILES ${PUB_HEADER}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/e4pipe"
    COMPONENT e4pipe_Development
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/e4pipeConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/e4pipeConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/e4pipeConfig.cmake"
    INSTALL_DESTINATION "${E4PIPE_INSTALL_CMAKEDIR}"
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/e4pipeConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/e4pipeConfigVersion.cmake"
    DESTINATION "${E4PIPE_INSTALL_CMAKEDIR}"
    COMPONENT e4pipe_Development
)

install(EXPORT e4pipeTargets
    FILE e4pipeTargets.cmake
    NAMESPACE E4Pipe::
    DESTINATION "${E4PIPE_INSTALL_CMAKEDIR}"
    COMPONENT e4pipe_Development
)

export(EXPORT e4pipeTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/e4pipeTargets.cmake"
    NAMESPACE E4Pipe::
)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_COMPONENTS_GROUPING IGNORE)
    set(CPACK_PACKAGE_NAME "e4pipe")
    set(CPACK_PACKAGE_CONTACT "${E4PIPE_PACKAGE_MAINTAINER}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${E4PIPE_PACKAGE_MAINTAINER}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Linux helper library with infinitypipe and libevent-backed pipeevent")

    if (E4PIPE_LIBRARY_STATIC)
        set(CPACK_COMPONENTS_ALL e4pipe_Development)
        set(CPACK_DEBIAN_DEV_PACKAGE_NAME "e4pipe-dev")
        set(CPACK_DEBIAN_DEV_PACKAGE_SECTION "libdevel")
        set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "libevent-dev")
    else()
        set(CPACK_COMPONENTS_ALL e4pipe_Runtime;e4pipe_Development)
        set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "e4pipe")
        set(CPACK_DEBIAN_RUNTIME_PACKAGE_SECTION "libs")
        set(CPACK_DEBIAN_RUNTIME_PACKAGE_SHLIBDEPS ON)
        set(CPACK_DEBIAN_DEV_PACKAGE_NAME "e4pipe-dev")
        set(CPACK_DEBIAN_DEV_PACKAGE_SECTION "libdevel")
        set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS "e4pipe (= ${PROJECT_VERSION}), libevent-dev")
    endif()

    include(CPack)
endif()
