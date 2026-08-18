cmake_minimum_required(VERSION 3.21)
project(imPlayer VERSION 0.1.2)

if (NOT MSVC)
    message(FATAL_ERROR "This project is intended to be built with MSVC")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(IPLAYER_ARCH x64)
else()
    set(IPLAYER_ARCH x86)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

set(OUTPUT_ROOT ${CMAKE_BINARY_DIR}/bin)

foreach(cfg Debug Release)
    string(TOUPPER ${cfg} CFG_UPPER)
#    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} ${OUTPUT_ROOT}/${cfg})
#    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} ${OUTPUT_ROOT}/${cfg})
#    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} ${OUTPUT_ROOT}/${cfg})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} ${OUTPUT_ROOT})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} ${OUTPUT_ROOT})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${CFG_UPPER} ${OUTPUT_ROOT})
endforeach()

include(cmake/msvc_options.cmake)

# thirdparty root
set(THIRDLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

# file(GLOB_RECURSE SRC_FILES src/*.cpp)
file(GLOB SRC_FILES src/*.cpp)

add_subdirectory(src/core)
add_subdirectory(src/decoder)
add_subdirectory(src/encoder)
add_subdirectory(src/player)
add_subdirectory(libsnd_decoder)
add_subdirectory(libvgm_decoder)
add_subdirectory(gme_decoder)
add_subdirectory(mpg123_decoder)
add_subdirectory(libflac_decoder)
add_subdirectory(libogg_decoder)
add_subdirectory(wavpack_decoder)
add_subdirectory(ffmpeg_decoder)
add_subdirectory(cdtrack_decoder)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/core)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/decoder)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include/player)


# add_executable(${PROJECT_NAME}  ${SRC_FILES})
target_link_libraries(${PROJECT_NAME} PRIVATE core decoder encoder player)
add_dependencies(${PROJECT_NAME} libsnd_decoder)
add_dependencies(${PROJECT_NAME} libvgm_decoder)
add_dependencies(${PROJECT_NAME} gme_decoder)
add_dependencies(${PROJECT_NAME} mpg123_decoder)
add_dependencies(${PROJECT_NAME} libflac_decoder)
add_dependencies(${PROJECT_NAME} libogg_decoder)
add_dependencies(${PROJECT_NAME} wavpack_decoder)
add_dependencies(${PROJECT_NAME} ffmpeg_decoder)
add_dependencies(${PROJECT_NAME} cdtrack_decoder)
target_link_libraries(${PROJECT_NAME} PRIVATE
        ole32
        uuid
        winmm
        dsound
        avrt       # MMCSS（低延迟音频）
)

# find_package(nlohmann_json CONFIG REQUIRED)
#   target_link_libraries(core PRIVATE nlohmann_json::nlohmann_json)

# list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")


 find_package(ICU REQUIRED COMPONENTS uc i18n data)
  target_link_libraries(${PROJECT_NAME} PRIVATE ICU::uc ICU::i18n ICU::data)


# Debug 专用定义

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)
