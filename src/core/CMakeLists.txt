cmake_minimum_required(VERSION 3.21)

project(core)

add_library(core STATIC
    AudioInfo.cpp
    AudioCD.cpp
    StringEx.cpp
    UnicodeNormalize.cpp
    UnicodeConvert.cpp
    FileStream.cpp
    CDSectorsStream.cpp
    MemBufStream.cpp
    ConvertFormat.cpp
    AudioConvert.cpp
    MediaTag.cpp
    MediaTagNames.cpp
    Utf8String.cpp
    StdFileSystem.cpp
    RiffUtils.cpp
)

target_include_directories(core PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(core PRIVATE ${CMAKE_SOURCE_DIR}/include/core)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# message(STATUS "CMAKE_MODULE_PATH = ${CMAKE_MODULE_PATH}")
if(MSVC)
    include(msvc_options)
    message(STATUS "MSVC options applied")
endif()


find_package(nlohmann_json CONFIG REQUIRED)
  target_link_libraries(core PRIVATE nlohmann_json::nlohmann_json)

find_package(ICU REQUIRED COMPONENTS uc i18n data)
  target_link_libraries(core PRIVATE ICU::uc ICU::i18n ICU::data)


find_package(FFMPEG REQUIRED)
  target_include_directories(core PRIVATE ${FFMPEG_INCLUDE_DIRS})
  target_link_directories(core PRIVATE ${FFMPEG_LIBRARY_DIRS})
  target_link_libraries(core PRIVATE ${FFMPEG_LIBRARIES})

find_package(libcdio REQUIRED)
  target_link_libraries(core PRIVATE libcdio::libcdio)

# get_target_property(_incs core INCLUDE_DIRECTORIES)
# message(STATUS "core includes = ${_incs}")


