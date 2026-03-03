cmake_minimum_required(VERSION 3.20)
project(ViewCam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── Find Dependencies ──────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Svg LinguistTools)
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
    src/ui/AudioTab.cpp
    src/ui/SettingsTab.cpp
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
    include/ui/AudioTab.h
    include/ui/SettingsTab.h
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
    Qt6::Svg
    ${SPDLOG_LIBRARIES}
)

# ── Deploy (portable bin/ directory) ──────────────────────────
set(DEPLOY_DIR "${CMAKE_SOURCE_DIR}/bin")
set(DEPLOY_LIB_DIR "${DEPLOY_DIR}/lib")
set(DEPLOY_PLUGINS_DIR "${DEPLOY_DIR}/plugins/platforms")
set(DEPLOY_RESOURCES_DIR "${DEPLOY_DIR}/resources/icons")
set(DEPLOY_TRANSLATIONS_DIR "${DEPLOY_DIR}/translations")

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${DEPLOY_DIR}"
)

# Resolve Qt library and plugin paths
get_target_property(_qt_core_loc Qt6::Core LOCATION)
get_filename_component(QT_LIB_DIR "${_qt_core_loc}" DIRECTORY)
find_path(QT_PLUGINS_DIR NAMES platforms
    PATHS
        "${QT_LIB_DIR}/../plugins"
        "${QT_LIB_DIR}/qt6/plugins"
        "${QT_LIB_DIR}/qt/plugins"
    NO_DEFAULT_PATH
)

# Resolve spdlog/fmt library paths
pkg_get_variable(SPDLOG_LIBDIR spdlog libdir)

# Create directory structure at build time
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    # Create directories
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_LIB_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_PLUGINS_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_RESOURCES_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_TRANSLATIONS_DIR}"

    # Copy Qt shared libraries
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${QT_LIB_DIR}/libQt6Core.so.6"
        "${QT_LIB_DIR}/libQt6Widgets.so.6"
        "${QT_LIB_DIR}/libQt6Network.so.6"
        "${QT_LIB_DIR}/libQt6Gui.so.6"
        "${QT_LIB_DIR}/libQt6DBus.so.6"
        "${QT_LIB_DIR}/libQt6Svg.so.6"
        "${DEPLOY_LIB_DIR}"

    # Copy spdlog + fmt shared libraries
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPDLOG_LIBDIR}/libspdlog.so"
        "${SPDLOG_LIBDIR}/libfmt.so"
        "${DEPLOY_LIB_DIR}"

    COMMENT "Deploying shared libraries to bin/lib/"
)

# Copy Qt platform plugins (safe — skips missing ones)
set(_qt_platform_plugins libqxcb.so libqwayland.so)
foreach(_plugin ${_qt_platform_plugins})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -Dsrc=${QT_PLUGINS_DIR}/platforms/${_plugin}
            -Ddst=${DEPLOY_PLUGINS_DIR}
            -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_exists.cmake
        COMMENT "Copying platform plugin ${_plugin} (if present)"
    )
endforeach()

# Copy SVG icon plugin (needed for QIcon to render SVG)
set(DEPLOY_IMAGEFORMATS_DIR "${DEPLOY_DIR}/plugins/imageformats")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_IMAGEFORMATS_DIR}"
    COMMAND ${CMAKE_COMMAND}
        -Dsrc=${QT_PLUGINS_DIR}/imageformats/libqsvg.so
        -Ddst=${DEPLOY_IMAGEFORMATS_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/copy_if_exists.cmake
    COMMENT "Copying SVG icon plugin (if present)"
)

# Generate launcher script at configure time
file(WRITE "${DEPLOY_DIR}/viewcam.sh"
[=[#!/bin/bash
# ViewCam launcher — sets up library paths for portable deployment
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$SCRIPT_DIR/plugins"
exec "$SCRIPT_DIR/ViewCam" "$@"
]=])
file(CHMOD "${DEPLOY_DIR}/viewcam.sh"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
)

# Copy app icon from project root
file(COPY "${CMAKE_SOURCE_DIR}/../Logo.svg"
     DESTINATION "${DEPLOY_DIR}/resources/icons/")
file(RENAME "${DEPLOY_DIR}/resources/icons/Logo.svg"
     "${DEPLOY_DIR}/resources/icons/viewcam.svg")

# ── Translations ──────────────────────────────────────────────
set(TS_FILES
    translations/viewcam_en.ts
    translations/viewcam_tr.ts
    translations/viewcam_de.ts
    translations/viewcam_fr.ts
)

# Compile .ts -> .qm
foreach(_ts ${TS_FILES})
    get_filename_component(_ts_name "${_ts}" NAME_WE)
    set(_qm "${CMAKE_BINARY_DIR}/${_ts_name}.qm")
    add_custom_command(
        OUTPUT "${_qm}"
        COMMAND Qt6::lrelease "${CMAKE_SOURCE_DIR}/${_ts}" -qm "${_qm}"
        DEPENDS "${CMAKE_SOURCE_DIR}/${_ts}"
        COMMENT "Compiling ${_ts} -> ${_ts_name}.qm"
    )
    list(APPEND QM_FILES "${_qm}")
endforeach()

add_custom_target(translations ALL DEPENDS ${QM_FILES})
add_dependencies(${PROJECT_NAME} translations)

# Deploy .qm files to bin/translations/
foreach(_qm ${QM_FILES})
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_qm}" "${DEPLOY_TRANSLATIONS_DIR}"
        COMMENT "Deploying ${_qm} to bin/translations/"
    )
endforeach()

# ── Install ────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
