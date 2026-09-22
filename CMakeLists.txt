cmake_minimum_required(VERSION 3.24)

project(VidView LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)

pkg_check_modules(
    FFMPEG
    REQUIRED
    IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
)

add_executable(vidview WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/VideoDecoder.cpp
    src/VideoDecoder.h
)

target_link_libraries(vidview
    PRIVATE
        Qt6::Widgets
        PkgConfig::FFMPEG
)