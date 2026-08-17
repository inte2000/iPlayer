cmake_minimum_required(VERSION 3.21)

project(imPlayerTests)

find_package(Catch2 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(imPlayerTests
    main.cpp
    core/CoreTests.cpp
    core/FileStreamMateSourceTests.cpp
    core/MediaTagTests.cpp
    core/UnicodeConvertTests.cpp
    core/UnicodeNormalizeTests.cpp
    core/Utf8StringTests.cpp
    decoder/AudioDecoderMapTests.cpp
    decoder/DecoderTests.cpp
    decoder/DecoderFactoryTests.cpp
    decoder/PluginConfigTests.cpp
    encoder/EncoderTests.cpp
    player/PlayerTests.cpp
)

target_include_directories(imPlayerTests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/core
    ${CMAKE_SOURCE_DIR}/include/decoder
    ${CMAKE_SOURCE_DIR}/include/encoder
    ${CMAKE_SOURCE_DIR}/include/player
)

target_link_libraries(imPlayerTests PRIVATE
    Catch2::Catch2
    nlohmann_json::nlohmann_json
    core
    decoder
    encoder
    player
    ole32
    uuid
    winmm
    dsound
    avrt
)

set_target_properties(imPlayerTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_ROOT}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_ROOT}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_ROOT}
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

if(MSVC)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/msvc_options.cmake")
        include(msvc_options)
        message(STATUS "MSVC options applied to libsnd_decoder")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(Catch)
catch_discover_tests(imPlayerTests)
