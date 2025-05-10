cmake_minimum_required(VERSION 3.20)
project(ViewCam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Find Dependencies ──────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SPDLOG REQUIRED spdlog)

# ── Sources ────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/Settings.cpp
    src/core/Logger.cpp
    src/ui/MainWindow.cpp
    src/ui/CameraPreviewWidget.cpp
    src/ui/ConnectionPanel.cpp
    src/network/StreamReceiver.cpp
    src/network/DeviceDiscovery.cpp
    src/network/FrameDecoder.cpp
)

set(HEADERS
    include/core/Application.h
    include/core/Settings.h
    include/core/FrameData.h
    include/core/Logger.h
    include/ui/MainWindow.h
    include/ui/CameraPreviewWidget.h
    include/ui/ConnectionPanel.h
    include/network/StreamReceiver.h
    include/network/DeviceDiscovery.h
    include/network/FrameDecoder.h
)

# ── Virtual Camera (platform-specific) ───────────────────────
if(LINUX)
    list(APPEND SOURCES src/virtualcam/linux/V4L2LoopbackWriter.cpp)
    list(APPEND HEADERS include/virtualcam/V4L2LoopbackWriter.h)
endif()

# ── Build ──────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SPDLOG_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    ${SPDLOG_LIBRARIES}
)

# ── Install ────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
