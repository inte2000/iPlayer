cmake_minimum_required(VERSION 3.21)

project(cdtrack_decoder)

add_library(cdtrack_decoder SHARED
    PlugMain.cpp
)

target_include_directories(cdtrack_decoder PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/core
)

if(MSVC)
    target_link_options(cdtrack_decoder PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/cdtrack_decoder.def")
endif()

set_target_properties(cdtrack_decoder PROPERTIES
    SUFFIX ".ipdplus"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/plugins"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/plugins"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(MSVC)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/msvc_options.cmake")
        include(msvc_options)
        message(STATUS "MSVC options applied to cdtrack_decoder")
    endif()
endif()

target_link_libraries(cdtrack_decoder PRIVATE
    core
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
