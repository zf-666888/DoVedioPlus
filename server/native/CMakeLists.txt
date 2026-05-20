cmake_minimum_required(VERSION 3.15)
project(DoVedioPlus VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libavutil libswscale libswresample
)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)

find_package(JNI REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

set(SOURCES
    src/video_processor.cpp
    src/ffmpeg_wrapper.cpp
    src/jni_bridge.cpp
    src/downloader.cpp
)

set(HEADERS
    include/video_processor.h
    include/ffmpeg_wrapper.h
    include/jni_bridge.h
    include/downloader.h
)

add_library(dovedioplus SHARED ${SOURCES})

target_include_directories(dovedioplus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JNI_INCLUDE_DIRS}
    ${FFMPEG_INCLUDE_DIRS}
)

target_link_libraries(dovedioplus PRIVATE
    ${FFMPEG_LIBRARIES}
    ${JNI_LIBRARIES}
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

target_compile_options(dovedioplus PRIVATE -O3 -march=native)
