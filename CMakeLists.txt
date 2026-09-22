cmake_minimum_required(VERSION 3.24)

project(
    VidView
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(
    Qt6
    REQUIRED
    COMPONENTS
        Widgets
        Multimedia
        OpenGL
        OpenGLWidgets
)

find_package(
    PkgConfig
    REQUIRED
)

pkg_check_modules(
    FFMPEG
    REQUIRED
    IMPORTED_TARGET
    libavformat
    libavcodec
    libavutil
    libswscale
    libswresample
)

set(
    VIDVIEW_PLATFORM_SOURCES
)

if(WIN32)
    enable_language(RC)

    configure_file(
        resources/VidView.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/VidView.rc
        @ONLY
    )

    list(
        APPEND
        VIDVIEW_PLATFORM_SOURCES
        ${CMAKE_CURRENT_BINARY_DIR}/VidView.rc
    )
endif()

add_executable(
    vidview
    WIN32

    src/main.cpp

    src/MainWindow.cpp
    src/MainWindow.h

    src/DecoderWorker.cpp
    src/DecoderWorker.h

    src/VideoWidget.cpp
    src/VideoWidget.h

    src/LoopSlider.cpp
    src/LoopSlider.h

    resources/VidView.qrc

    ${VIDVIEW_PLATFORM_SOURCES}
)

set_target_properties(
    vidview
    PROPERTIES
        OUTPUT_NAME "VidView"
)

target_compile_definitions(
    vidview
    PRIVATE
        VIDVIEW_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(
    vidview
    PRIVATE
        Qt6::Widgets
        Qt6::Multimedia
        Qt6::OpenGL
        Qt6::OpenGLWidgets
        PkgConfig::FFMPEG
)