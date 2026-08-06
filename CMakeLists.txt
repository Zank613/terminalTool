# SPDX-License-Identifier: MPL-2.0
# Copyright (c) 2026 Ataerk YILDIRIM

cmake_minimum_required(VERSION 3.20)
project(terminalTool VERSION 0.3.1 LANGUAGES CXX)

message(STATUS "Configuring terminalTool ${PROJECT_VERSION}")

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(TERMINALTOOL_DEVELOPMENT_DEFAULT ON)
else()
    set(TERMINALTOOL_DEVELOPMENT_DEFAULT OFF)
endif()

option(TERMINALTOOL_BUILD_EXAMPLE "Build the terminalTool demonstration" ${TERMINALTOOL_DEVELOPMENT_DEFAULT})
option(TERMINALTOOL_BUILD_DOCS "Create the terminalToolDocs Doxygen target when Doxygen is available" ${TERMINALTOOL_DEVELOPMENT_DEFAULT})
option(TERMINALTOOL_BUILD_TESTS "Build terminalTool's self-contained unit tests" ${TERMINALTOOL_DEVELOPMENT_DEFAULT})

# A generated Version.h from an older archive must never be stored in source.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/terminalTool/Version.h")
    message(FATAL_ERROR
        "A stale include/terminalTool/Version.h exists in the source tree. "
        "Delete it; terminalTool generates Version.h in the build directory."
    )
endif()

set(TERMINALTOOL_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${TERMINALTOOL_GENERATED_INCLUDE_DIR}/terminalTool")

configure_file(
    include/terminalTool/Version.h.in
    "${TERMINALTOOL_GENERATED_INCLUDE_DIR}/terminalTool/Version.h"
    @ONLY
)
configure_file(
    cmake/VerifyGeneratedVersion.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/VerifyGeneratedVersion.cmake"
    @ONLY
)

# Verify immediately during configuration, then expose the same version-agnostic
# check as a build target and CTest test. CI never hard-codes a release number.
execute_process(
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/VerifyGeneratedVersion.cmake"
    RESULT_VARIABLE TERMINALTOOL_VERSION_CHECK_RESULT
)
if(NOT TERMINALTOOL_VERSION_CHECK_RESULT EQUAL 0)
    message(FATAL_ERROR "terminalTool generated version verification failed.")
endif()

add_custom_target(terminalToolVersionCheck
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/VerifyGeneratedVersion.cmake"
    COMMENT "Verifying terminalTool generated version metadata"
    VERBATIM
)

set(TERMINALTOOL_SOURCES
    src/Colour.cpp
    src/Console.cpp
    src/DeltaTime.cpp
    src/Input.cpp
    src/TerminalError.cpp
    src/TerminalSession.cpp
    src/detail/EscapeSequenceParser.cpp
    src/detail/Unicode.cpp
    src/detail/Utf16.cpp
)

if(WIN32)
    list(APPEND TERMINALTOOL_SOURCES src/platform/WindowsTerminal.cpp)
else()
    list(APPEND TERMINALTOOL_SOURCES src/platform/PosixTerminal.cpp)
endif()

# terminalTool remains deliberately static-only.
add_library(terminalTool STATIC ${TERMINALTOOL_SOURCES})
add_library(terminalTool::terminalTool ALIAS terminalTool)
add_dependencies(terminalTool terminalToolVersionCheck)

target_compile_features(terminalTool PUBLIC cxx_std_17)
set_target_properties(terminalTool PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME terminalTool
)

target_include_directories(terminalTool
    PUBLIC
        $<BUILD_INTERFACE:${TERMINALTOOL_GENERATED_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(terminalTool PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(terminalTool PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_compile_definitions(terminalTool PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(TERMINALTOOL_BUILD_EXAMPLE)
    add_executable(terminalToolDemo examples/demo.cpp)
    target_link_libraries(terminalToolDemo PRIVATE terminalTool::terminalTool)

    if(MSVC)
        target_compile_options(terminalToolDemo PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(terminalToolDemo PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(BUILD_TESTING AND TERMINALTOOL_BUILD_TESTS)
    add_executable(terminalToolTests tests/terminalToolTests.cpp)
    target_link_libraries(terminalToolTests PRIVATE terminalTool::terminalTool)
    target_include_directories(terminalToolTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(terminalToolTests PRIVATE
        TERMINALTOOL_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        TERMINALTOOL_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        TERMINALTOOL_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        TERMINALTOOL_EXPECTED_VERSION_STRING="${PROJECT_VERSION}"
    )

    add_executable(terminalToolParserTests tests/escapeSequenceParserTests.cpp)
    target_link_libraries(terminalToolParserTests PRIVATE terminalTool::terminalTool)
    target_include_directories(terminalToolParserTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

    add_executable(terminalToolPublicHeaderSmoke tests/publicHeaderSmoke.cpp)
    target_link_libraries(terminalToolPublicHeaderSmoke PRIVATE terminalTool::terminalTool)

    foreach(testTarget terminalToolTests terminalToolParserTests terminalToolPublicHeaderSmoke)
        if(MSVC)
            target_compile_options(${testTarget} PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(${testTarget} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME terminalToolGeneratedVersion
        COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/VerifyGeneratedVersion.cmake"
    )
    add_test(NAME terminalToolTests COMMAND terminalToolTests)
    add_test(NAME terminalToolParserTests COMMAND terminalToolParserTests)
    add_test(NAME terminalToolPublicHeaderSmoke COMMAND terminalToolPublicHeaderSmoke)

    if(UNIX)
        find_package(Python3 COMPONENTS Interpreter QUIET)
        if(Python3_Interpreter_FOUND)
            add_executable(terminalToolPosixSessionProbe tests/posixSessionProbe.cpp)
            target_link_libraries(terminalToolPosixSessionProbe PRIVATE terminalTool::terminalTool)
            target_compile_features(terminalToolPosixSessionProbe PRIVATE cxx_std_17)
            if(MSVC)
                target_compile_options(terminalToolPosixSessionProbe PRIVATE /W4 /permissive- /utf-8)
            else()
                target_compile_options(terminalToolPosixSessionProbe PRIVATE -Wall -Wextra -Wpedantic)
            endif()
            add_test(
                NAME terminalToolPosixPtyTests
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/posixPtyTests.py" $<TARGET_FILE:terminalToolPosixSessionProbe>
            )
            set_tests_properties(terminalToolPosixPtyTests PROPERTIES TIMEOUT 20)
        endif()
    endif()
endif()

if(TERMINALTOOL_BUILD_DOCS)
    find_package(Doxygen QUIET)
    if(Doxygen_FOUND)
        configure_file(docs/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        add_custom_target(terminalToolDocs
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
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "Version.h" EXCLUDE
)

install(
    FILES "${TERMINALTOOL_GENERATED_INCLUDE_DIR}/terminalTool/Version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/terminalTool
)

install(FILES LICENSE NOTICE.md CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/terminalTool)
install(DIRECTORY docs/ DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/terminalTool/docs FILES_MATCHING PATTERN "*.md")

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
