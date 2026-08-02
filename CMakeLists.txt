# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Ataerk YILDIRIM

cmake_minimum_required(VERSION 3.20)
project(terminalTool VERSION 0.2.1 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(TERMINALTOOL_BUILD_EXAMPLE "Build the terminalTool demonstration" ON)
option(TERMINALTOOL_BUILD_DOCS "Create a Doxygen documentation target when Doxygen is available" ON)
option(TERMINALTOOL_BUILD_TESTS "Build terminalTool's self-contained unit tests" ON)

set(TERMINALTOOL_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${TERMINALTOOL_GENERATED_INCLUDE_DIR}/terminalTool")

configure_file(
    include/terminalTool/Version.h.in
    "${TERMINALTOOL_GENERATED_INCLUDE_DIR}/terminalTool/Version.h"
    @ONLY
)

# terminalTool is deliberately static-only. BUILD_SHARED_LIBS does not alter it.
add_library(terminalTool STATIC
    src/Colour.cpp
    src/Console.cpp
    src/Input.cpp
    src/TerminalError.cpp
    src/TerminalSession.cpp
)

add_library(terminalTool::terminalTool ALIAS terminalTool)

target_compile_features(terminalTool PUBLIC cxx_std_17)
set_target_properties(terminalTool PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME terminalTool
)

target_include_directories(terminalTool
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${TERMINALTOOL_GENERATED_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if (MSVC)
    target_compile_options(terminalTool PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(terminalTool PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (WIN32)
    target_compile_definitions(terminalTool PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (TERMINALTOOL_BUILD_EXAMPLE)
    add_executable(terminalToolDemo examples/demo.cpp)
    target_link_libraries(terminalToolDemo PRIVATE terminalTool::terminalTool)

    if (MSVC)
        target_compile_options(terminalToolDemo PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(terminalToolDemo PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if (TERMINALTOOL_BUILD_TESTS)
    add_executable(terminalToolTests tests/terminalToolTests.cpp)
    target_link_libraries(terminalToolTests PRIVATE terminalTool::terminalTool)

    if (MSVC)
        target_compile_options(terminalToolTests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(terminalToolTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME terminalToolTests COMMAND terminalToolTests)

    add_executable(terminalToolPublicHeaderSmoke tests/publicHeaderSmoke.cpp)
    target_link_libraries(terminalToolPublicHeaderSmoke PRIVATE terminalTool::terminalTool)

    if (MSVC)
        target_compile_options(terminalToolPublicHeaderSmoke PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(terminalToolPublicHeaderSmoke PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME terminalToolPublicHeaderSmoke COMMAND terminalToolPublicHeaderSmoke)
endif()

if (TERMINALTOOL_BUILD_DOCS)
    find_package(Doxygen QUIET)

    if (Doxygen_FOUND)
        configure_file(docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating terminalTool API documentation"
            VERBATIM
        )
    endif()
endif()

install(TARGETS terminalTool
    EXPORT terminalToolTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    DIRECTORY include/terminalTool/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/terminalTool
    FILES_MATCHING PATTERN "*.h"
)

install(
    FILES "${TERMINALTOOL_GENERATED_INCLUDE_DIR}/terminalTool/Version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/terminalTool
)

install(FILES LICENSE NOTICE.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/terminalTool)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/terminalToolConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion
)

configure_package_config_file(
    cmake/terminalToolConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/terminalToolConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/terminalTool
)

install(EXPORT terminalToolTargets
    FILE terminalToolTargets.cmake
    NAMESPACE terminalTool::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/terminalTool
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/terminalToolConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/terminalToolConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/terminalTool
)
