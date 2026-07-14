cmake_minimum_required(VERSION 3.20)
project(RailShotBroadcaster VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/resources/RailShotBroadcaster.rc.in"
    "${CMAKE_BINARY_DIR}/generated/RailShotBroadcaster.rc"
    @ONLY
)

# ---------------------------------------------------------------------------
# Vendor paths — place third-party binaries under /vendor
# ---------------------------------------------------------------------------
set(VENDOR_DIR "${CMAKE_SOURCE_DIR}/vendor" CACHE PATH "Vendor directory")

# Qt 6 — aqtinstall places MSVC builds under 6.x.x/msvc2022_64
set(QT_ROOT "${VENDOR_DIR}/qt/6.8.2/msvc2022_64" CACHE PATH "Qt 6 root")
list(APPEND CMAKE_PREFIX_PATH
    "${QT_ROOT}"
    "${VENDOR_DIR}/qt"
    "$ENV{QTDIR}"
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets Network WebSockets)

# FFmpeg
set(FFMPEG_ROOT "${VENDOR_DIR}/ffmpeg" CACHE PATH "FFmpeg root")
find_path(FFMPEG_INCLUDE_DIR libavcodec/avcodec.h
    HINTS "${FFMPEG_ROOT}/include" "${FFMPEG_ROOT}"
)
find_library(AVCODEC_LIB  avcodec  HINTS "${FFMPEG_ROOT}/lib")
find_library(AVFORMAT_LIB avformat HINTS "${FFMPEG_ROOT}/lib")
find_library(AVUTIL_LIB   avutil   HINTS "${FFMPEG_ROOT}/lib")
find_library(SWSCALE_LIB  swscale  HINTS "${FFMPEG_ROOT}/lib")
find_library(SWRESAMPLE_LIB swresample HINTS "${FFMPEG_ROOT}/lib")

if(NOT FFMPEG_INCLUDE_DIR OR NOT AVCODEC_LIB)
    message(FATAL_ERROR
        "FFmpeg not found. Place binaries in ${VENDOR_DIR}/ffmpeg "
        "(include/ and lib/ subdirectories).")
endif()

# librtmp
set(LIBRTMP_ROOT "${VENDOR_DIR}/librtmp" CACHE PATH "librtmp root")
find_path(LIBRTMP_INCLUDE_DIR rtmp.h
    HINTS "${LIBRTMP_ROOT}/include" "${LIBRTMP_ROOT}"
)
find_library(LIBRTMP_LIB rtmp HINTS "${LIBRTMP_ROOT}/lib")

if(NOT LIBRTMP_INCLUDE_DIR OR NOT LIBRTMP_LIB)
    message(FATAL_ERROR
        "librtmp not found. Place binaries in ${VENDOR_DIR}/librtmp "
        "(include/ and lib/ subdirectories).")
endif()

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
set(SOURCES
    src/main.cpp
    "${CMAKE_BINARY_DIR}/generated/RailShotBroadcaster.rc"
    src/core/Logger.cpp
    src/core/FrameData.cpp
    src/core/GLCompositor.cpp
    src/core/StreamController.cpp
    src/core/SourceRegistry.cpp
    src/core/AudioMixer.cpp
    src/core/AppSettings.cpp
    src/core/RemoteCommandBus.cpp
    src/core/models/SceneManager.cpp
    src/core/models/FilterUtils.cpp
    src/capture/DirectShowCapture.cpp
    src/capture/WasapiAudioCapture.cpp
    src/capture/CaptureConvert.cpp
    src/capture/VideoDeviceSource.cpp
    src/capture/ImageSource.cpp
    src/capture/MediaSource.cpp
    src/capture/MediaSourceSettings.cpp
    src/capture/NdiFinder.cpp
    src/capture/NdiSource.cpp
    src/capture/BrowserSource.cpp
    src/capture/BrowserSourceSettings.cpp
    src/capture/WebView2BrowserHost.cpp
    src/capture/ScoreboardSource.cpp
    src/capture/ColorSource.cpp
    src/capture/TextSource.cpp
    src/capture/DesktopAudioSource.cpp
    src/capture/AudioDeviceSource.cpp
    src/capture/ApplicationAudioSource.cpp
    src/capture/WgcWindowCapture.cpp
    src/capture/GameCaptureSource.cpp
    src/capture/DxgiMonitorCapture.cpp
    src/capture/DisplayCaptureSource.cpp
    src/capture/WindowBitbltCapture.cpp
    src/capture/WindowCaptureSource.cpp
    src/capture/WasapiAudioMonitor.cpp
    src/score/ScoreManager.cpp
    src/score/ScoreboardStyle.cpp
    src/score/ScoreboardRenderer.cpp
    src/network/LocalWebServer.cpp
    src/network/RemoteWebSocketServer.cpp
    src/network/RailShotApiClient.cpp
    src/encoder/FFmpegEncoder.cpp
    src/output/RtmpOutput.cpp
    src/output/FileRecorder.cpp
    src/output/ReplayBuffer.cpp
    src/output/Remux.cpp
    src/output/IsoRecorder.cpp
    src/output/VirtualCamOutput.cpp
    src/output/virtualcam/shared-memory-queue.c
    src/output/virtualcam/tiny-nv12-scale.c
    src/core/StreamServicePresets.cpp
    src/ui/BrowserSourceConfigDialog.cpp
    src/ui/ColorSourceConfigDialog.cpp
    src/ui/MediaSourceConfigDialog.cpp
    src/ui/TextSourceConfigDialog.cpp
    src/ui/AddSourceDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/FiltersDialog.cpp
    src/ui/MainWindow.cpp
    src/ui/DockPanel.cpp
    src/ui/PreviewWidget.cpp
    src/ui/AudioMixer.cpp
    src/ui/StudioMode.cpp
    src/ui/ScoreboardWidget.cpp
    src/ui/ScoreboardConfigDialog.cpp
    src/ui/ScoreboardPreviewWidget.cpp
    src/ui/VenueMode.cpp
)

# Optional NDI SDK (vendor/ndi)
set(NDI_ROOT "${VENDOR_DIR}/ndi" CACHE PATH "NDI SDK root")
find_path(NDI_INCLUDE_DIR Processing.NDI.Lib.h
    HINTS "${NDI_ROOT}/include" "${NDI_ROOT}/Include"
)
find_library(NDI_LIB Processing.NDI.Lib.x64
    HINTS "${NDI_ROOT}/lib/x64" "${NDI_ROOT}/Lib/x64"
)

if(NDI_INCLUDE_DIR AND NDI_LIB)
    message(STATUS "NDI SDK found — enabling RAILSHOT_HAS_NDI")
endif()

# WebView2 (Chromium browser — OBS Browser Source equivalent on Windows)
set(WEBVIEW2_ROOT "${VENDOR_DIR}/webview2/package" CACHE PATH "WebView2 SDK root")
set(WEBVIEW2_INCLUDE_DIR "${WEBVIEW2_ROOT}/build/native/include")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WEBVIEW2_LOADER_DIR "${WEBVIEW2_ROOT}/build/native/x64")
else()
    set(WEBVIEW2_LOADER_DIR "${WEBVIEW2_ROOT}/build/native/x86")
endif()
set(WEBVIEW2_LOADER_LIB "${WEBVIEW2_LOADER_DIR}/WebView2LoaderStatic.lib")

add_executable(RailShotBroadcaster WIN32 ${SOURCES})

target_include_directories(RailShotBroadcaster PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIR}
    ${LIBRTMP_INCLUDE_DIR}
)

target_link_libraries(RailShotBroadcaster PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::Network
    Qt6::WebSockets
    ${AVCODEC_LIB}
    ${AVFORMAT_LIB}
    ${AVUTIL_LIB}
    ${SWSCALE_LIB}
    ${SWRESAMPLE_LIB}
    ${LIBRTMP_LIB}
    strmiids
    ole32
    oleaut32
    uuid
    avrt
    mmdevapi
    ws2_32
    winmm
    d3d11
    dxgi
    gdi32
    windowsapp
)

if(EXISTS "${WEBVIEW2_INCLUDE_DIR}/WebView2.h" AND EXISTS "${WEBVIEW2_LOADER_LIB}")
    message(STATUS "WebView2 SDK found — enabling RAILSHOT_HAS_WEBVIEW2")
    target_include_directories(RailShotBroadcaster PRIVATE ${WEBVIEW2_INCLUDE_DIR})
    target_link_libraries(RailShotBroadcaster PRIVATE "${WEBVIEW2_LOADER_LIB}" version)
    target_compile_definitions(RailShotBroadcaster PRIVATE RAILSHOT_HAS_WEBVIEW2)
else()
    message(WARNING "WebView2 SDK not found in ${WEBVIEW2_ROOT} — browser source uses fallback renderer")
endif()

target_compile_definitions(RailShotBroadcaster PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
    RAILSHOT_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(RailShotBroadcaster PRIVATE /W4 /permissive-)
    target_link_options(RailShotBroadcaster PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/src/resources/RailShotBroadcaster.manifest"
    )
else()
    target_compile_options(RailShotBroadcaster PRIVATE -Wall -Wextra)
endif()

if(NDI_INCLUDE_DIR AND NDI_LIB)
    target_include_directories(RailShotBroadcaster PRIVATE ${NDI_INCLUDE_DIR})
    target_link_libraries(RailShotBroadcaster PRIVATE ${NDI_LIB})
    target_compile_definitions(RailShotBroadcaster PRIVATE RAILSHOT_HAS_NDI)
    add_custom_command(TARGET RailShotBroadcaster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${NDI_ROOT}/bin/x64/Processing.NDI.Lib.x64.dll"
            "$<TARGET_FILE_DIR:RailShotBroadcaster>"
        COMMENT "Copying NDI runtime DLL"
    )
endif()

# Copy Qt DLLs next to the executable after build
if(WIN32)
    add_custom_command(TARGET RailShotBroadcaster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:RailShotBroadcaster>/assets/overlays"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/assets/overlays/scoreboard.html"
            "$<TARGET_FILE_DIR:RailShotBroadcaster>/assets/overlays/scoreboard.html"
        COMMAND ${CMAKE_COMMAND} -E echo
            "Run windeployqt on the Release build before packaging."
    )
endif()

# ---------------------------------------------------------------------------
# RailShot Virtual Camera (64-bit DirectShow source filter)
# ---------------------------------------------------------------------------
add_library(railshot-virtualcam MODULE
    plugins/railshot-virtualcam/virtualcam-module.cpp
    plugins/railshot-virtualcam/virtualcam-filter.cpp
    plugins/railshot-virtualcam/placeholder.cpp
    plugins/railshot-virtualcam/sleepto.c
    plugins/railshot-virtualcam/dshow/output-filter.cpp
    plugins/railshot-virtualcam/dshow/dshow-formats.cpp
    plugins/railshot-virtualcam/dshow/dshow-media-type.cpp
    plugins/railshot-virtualcam/dshow/log.cpp
    src/output/virtualcam/shared-memory-queue.c
    src/output/virtualcam/tiny-nv12-scale.c
    plugins/railshot-virtualcam/railshot-virtualcam64.def
)

set_target_properties(railshot-virtualcam PROPERTIES
    OUTPUT_NAME railshot-virtualcam64
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

target_include_directories(railshot-virtualcam PRIVATE
    ${CMAKE_SOURCE_DIR}/plugins/railshot-virtualcam
    ${CMAKE_SOURCE_DIR}/plugins/railshot-virtualcam/dshow
    ${CMAKE_SOURCE_DIR}/src/output/virtualcam
)

target_compile_definitions(railshot-virtualcam PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

target_link_libraries(railshot-virtualcam PRIVATE
    strmiids
    ole32
    oleaut32
    uuid
    winmm
)

if(MSVC)
    target_compile_options(railshot-virtualcam PRIVATE /W3 /permissive-)
endif()

add_custom_command(TARGET railshot-virtualcam POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:railshot-virtualcam>"
        "$<TARGET_FILE_DIR:RailShotBroadcaster>/railshot-virtualcam64.dll"
    COMMENT "Copying virtual camera DLL next to exe"
)
add_dependencies(RailShotBroadcaster railshot-virtualcam)
