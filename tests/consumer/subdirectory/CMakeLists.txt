cmake_minimum_required(VERSION 3.20)
project(terminalToolSubdirectoryConsumer LANGUAGES CXX)

if(NOT DEFINED TERMINALTOOL_SOURCE_DIR)
    message(FATAL_ERROR "TERMINALTOOL_SOURCE_DIR is required")
endif()

add_subdirectory("${TERMINALTOOL_SOURCE_DIR}" terminalTool-build)

foreach(unexpectedTarget terminalToolDemo terminalToolTests terminalToolParserTests terminalToolPublicHeaderSmoke terminalToolDocs)
    if(TARGET ${unexpectedTarget})
        message(FATAL_ERROR "terminalTool subproject unexpectedly created target ${unexpectedTarget}")
    endif()
endforeach()

add_executable(terminalToolSubdirectoryConsumer main.cpp)
target_link_libraries(terminalToolSubdirectoryConsumer PRIVATE terminalTool::terminalTool)
target_compile_features(terminalToolSubdirectoryConsumer PRIVATE cxx_std_17)
