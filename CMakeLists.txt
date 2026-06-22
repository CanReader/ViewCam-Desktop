cmake_minimum_required(VERSION 3.20)
project(ViewCam
    VERSION 1.0.0
    DESCRIPTION "Turn your phone into a virtual webcam"
    HOMEPAGE_URL "https://viewcam.tech"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Product metadata (cache vars, overridable on the command line) ──
set(VIEWCAM_DISPLAY_NAME "ViewCam Studio"      CACHE STRING "User-facing product name")
set(VIEWCAM_EDITION      "Studio"              CACHE STRING "Product edition label")
set(VIEWCAM_ORG_NAME     "Sleak Software"      CACHE STRING "Organization name (QSettings)")
set(VIEWCAM_ORG_DOMAIN   "viewcam.io"          CACHE STRING "Organization domain")
set(VIEWCAM_BUNDLE_ID    "tech.viewcam.studio"   CACHE STRING "Reverse-DNS bundle identifier")
set(VIEWCAM_BUILD_NUMBER 1                     CACHE STRING "Monotonic build number")
set(VIEWCAM_COPYRIGHT    "(c) 2026 Sleak Software" CACHE STRING "Copyright line")
set(VIEWCAM_STREAM_PORT  8080                  CACHE STRING "Default TCP stream port")
set(VIEWCAM_BEACON_PORT  8081                  CACHE STRING "UDP discovery beacon port")

# ── Gather Metadata ──────────────────────────────────

# 1. Host Machine Information
string(TIMESTAMP VIEWCAM_BUILD_TIME "%Y-%m-%d %H:%M:%S UTC" UTC)

if(CMAKE_HOST_WIN32)
    set(VIEWCAM_BUILD_HOST $ENV{COMPUTERNAME})
    set(VIEWCAM_BUILD_USER $ENV{USERNAME})
else()
    set(VIEWCAM_BUILD_HOST $ENV{HOSTNAME})
    # Fallback shell command if environment variables are blank
    if(NOT VIEWCAM_BUILD_HOST)
        execute_process(COMMAND hostname OUTPUT_VARIABLE VIEWCAM_BUILD_HOST OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    execute_process(COMMAND whoami OUTPUT_VARIABLE VIEWCAM_BUILD_USER OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

# 2. Rich Git Analytics
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%h WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE VIEWCAM_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE VIEWCAM_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%ci WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE VIEWCAM_GIT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --format=%s WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE VIEWCAM_GIT_SUBJECT OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
    set(VIEWCAM_GIT_HASH "unknown")
    set(VIEWCAM_GIT_BRANCH "detached")
    set(VIEWCAM_GIT_DATE "unknown")
    set(VIEWCAM_GIT_SUBJECT "unknown")
endif()
if(VIEWCAM_GIT_HASH)
    set(VIEWCAM_GIT_SHA "${VIEWCAM_GIT_HASH}")
else()
    set(VIEWCAM_GIT_SHA "unknown")
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# 3. Read Spdlog Version cleanly if pkg-config found it
if(SPDLOG_VERSION)
    set(SPDLOG_VERSION_STR ${SPDLOG_VERSION})
else()
    set(SPDLOG_VERSION_STR "1.15.1 (FetchContent)")
endif()

# ── Find Dependencies ──────────────────────────────────────────
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick Network Svg LinguistTools)

qt_standard_project_setup(REQUIRES 6.5)

# ── Generated metadata header (single source of truth) ─────────
# Configured after find_package so Qt6_VERSION is known.
set(VIEWCAM_QT_VERSION "${Qt6_VERSION}")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ViewCamConfig.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/ViewCamConfig.h
    @ONLY)

# spdlog: pkg-config on Linux, find_package/FetchContent on Windows
if(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SPDLOG REQUIRED spdlog)
else()
    find_package(spdlog CONFIG QUIET)
    if(NOT spdlog_FOUND)
        include(FetchContent)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG        v1.15.1
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(spdlog)
    endif()
endif()

# ── GPU compute backends (optional, gated) ────────────────────
# Abstracted behind GpuBackend (see design/GPU_ABSTRACTION_BRIEF.md): CudaBackend
# (NVIDIA), VulkanComputeBackend (AMD/Intel/portable), CpuBackend (fallback).
# Runtime policy: NVIDIA -> CUDA, else Vulkan compute, else CPU.
#   Disable CUDA:   cmake -B build -DVIEWCAM_ENABLE_CUDA=OFF
#   Disable Vulkan: cmake -B build -DVIEWCAM_ENABLE_VULKAN_COMPUTE=OFF
# The app builds and runs with any combination (CPU fallback always works).
find_package(CUDAToolkit QUIET)
option(VIEWCAM_ENABLE_CUDA "Enable CUDA GPU compute (NVIDIA)" ${CUDAToolkit_FOUND})
set(VIEWCAM_HAVE_CUDA OFF)
if(VIEWCAM_ENABLE_CUDA AND CUDAToolkit_FOUND)
    enable_language(CUDA)
    # enable_language seeds CMAKE_CUDA_ARCHITECTURES with a compiler default, so
    # set our portable fatbin list unconditionally (Turing→Ada; 89 = RTX 4050).
    set(CMAKE_CUDA_ARCHITECTURES "75;86;89")
    set(VIEWCAM_HAVE_CUDA ON)
    message(STATUS "CUDA enabled (Toolkit ${CUDAToolkit_VERSION}, arch ${CMAKE_CUDA_ARCHITECTURES})")
else()
    message(STATUS "CUDA disabled (VIEWCAM_ENABLE_CUDA=OFF or CUDA Toolkit not found)")
endif()

find_package(Vulkan QUIET)
option(VIEWCAM_ENABLE_VULKAN_COMPUTE "Enable Vulkan compute backend (portable)" ${Vulkan_FOUND})
set(VIEWCAM_HAVE_VULKAN OFF)
if(VIEWCAM_ENABLE_VULKAN_COMPUTE AND Vulkan_FOUND)
    set(VIEWCAM_HAVE_VULKAN ON)
    message(STATUS "Vulkan compute enabled (${Vulkan_LIBRARIES})")
    # The proof-of-life compute shader is precompiled to embedded SPIR-V
    # (include/gpu/ProofComputeSpv.h). Regenerate after editing the GLSL:
    #   glslc src/gpu/shaders/proof.comp -o /tmp/proof.spv  (then re-embed)
else()
    message(STATUS "Vulkan compute disabled (option OFF or Vulkan not found)")
endif()

# ── Sources (backend + viewmodels, no widget UI) ──────────────
set(SOURCES
    src/main.cpp
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/resources/viewcam.rc>
    src/core/Settings.cpp
    src/core/Logger.cpp
    src/core/QmlDevMode.cpp
    src/network/StreamReceiver.cpp
    src/network/DeviceDiscovery.cpp
    src/network/FrameDecoder.cpp
    src/gpu/GpuBackendFactory.cpp
    src/gpu/CudaBackend.cpp
    src/gpu/VulkanComputeBackend.cpp
    src/updater/UpdateChecker.cpp
)

set(HEADERS
    include/core/Settings.h
    include/core/FrameData.h
    include/core/Logger.h
    include/core/QmlDevMode.h
    include/network/StreamReceiver.h
    include/network/DeviceDiscovery.h
    include/network/FrameDecoder.h
    include/gpu/GpuBackend.h
    include/gpu/GpuFrameFilter.h
    include/gpu/CpuBackend.h
    include/gpu/CudaBackend.h
    include/gpu/VulkanComputeBackend.h
    include/updater/UpdateChecker.h
)

# The CUDA proof-of-life kernel is compiled by nvcc only when CUDA is enabled.
if(VIEWCAM_HAVE_CUDA)
    list(APPEND SOURCES src/gpu/CudaProof.cu)
endif()

# ── Virtual Camera (platform-specific) ───────────────────────
if(LINUX)
    list(APPEND SOURCES src/virtualcam/linux/V4L2LoopbackWriter.cpp)
    list(APPEND HEADERS include/virtualcam/V4L2LoopbackWriter.h)
elseif(WIN32)
    list(APPEND SOURCES src/virtualcam/windows/DirectShowVirtualCam.cpp
                        src/virtualcam/windows/FilterRegistrar.cpp
                        src/virtualcam/windows/MFVirtualCamManager.cpp)
    list(APPEND HEADERS include/virtualcam/DirectShowVirtualCam.h
                        include/virtualcam/SharedFrameProtocol.h
                        include/virtualcam/FilterRegistrar.h
                        include/virtualcam/MFVirtualCamDefs.h
                        src/virtualcam/windows/MFVirtualCamManager.h)
endif()

# ── Build ──────────────────────────────────────────────────────
qt_add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Release on Windows: no console window when double-clicked.
# When launched from a terminal, main.cpp calls AttachConsole so stdout still works.
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
endif()

# ── QML module: ViewModels (QML_ELEMENT) + QML UI + assets ───
set(VIEWCAM_QML_FILES
    qml/Main.qml
    qml/AppContent.qml
    qml/Theme.qml
    qml/components/Monogram.qml
    qml/components/VcBadge.qml
    qml/components/VcButton.qml
    qml/components/VcProBadge.qml
    qml/components/VcProGateDialog.qml
    qml/components/VcCard.qml
    qml/components/VcDeviceCard.qml
    qml/components/VcDeviceRow.qml
    qml/components/VcIcon.qml
    qml/components/VcIconButton.qml
    qml/components/VcIpBox.qml
    qml/components/VcMeter.qml
    qml/components/VcNavItem.qml
    qml/components/VcOptRow.qml
    qml/components/VcPhoneGlyph.qml
    qml/components/VcSeg.qml
    qml/components/VcSettingRow.qml
    qml/components/VcSlider.qml
    qml/components/VcStatRow.qml
    qml/components/VcToggle.qml
    qml/components/VcTooltip.qml
    qml/screens/AppShell.qml
    qml/screens/Launcher.qml
    qml/screens/LiveView.qml
    qml/screens/RightPanel.qml
    qml/screens/Sidebar.qml
    qml/screens/SourcesPage.qml
    qml/screens/SettingsPage.qml
    qml/screens/UpdateScreen.qml
)

file(GLOB VIEWCAM_ICON_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} resources/icons/*.svg)

# Singleton QML components (need the "singleton" keyword in the qmldir)
set(VIEWCAM_QML_SINGLETONS Theme)

# Fonts/images bundled next to the qml inside the external blob
set(VIEWCAM_ASSET_FILES
    resources/images/Logo.svg
    resources/fonts/Geist-Regular.ttf
    resources/fonts/Geist-Medium.ttf
    resources/fonts/Geist-SemiBold.ttf
    resources/fonts/Geist-Bold.ttf
    resources/fonts/GeistMono-Regular.ttf
    resources/fonts/GeistMono-Medium.ttf
    ${VIEWCAM_ICON_FILES}
)

# Register ONLY the C++ QML_ELEMENT types into the executable. We deliberately
# pass NO QML_FILES / RESOURCES and NO_GENERATE_QMLDIR, so nothing UI-facing is
# embedded in the binary. The qml, qmldir, assets and translations all ship in
# the external, opaque viewcam.rcc (built below) and are registered at startup.
qt_add_qml_module(${PROJECT_NAME}
    URI ViewCam.Studio
    VERSION 1.0
    NO_GENERATE_QMLDIR
    SOURCES
        include/viewmodels/AppInfo.h
        src/viewmodels/AppInfo.cpp
        include/viewmodels/AppController.h
        src/viewmodels/AppController.cpp
        include/viewmodels/ConnectionViewModel.h
        src/viewmodels/ConnectionViewModel.cpp
        include/viewmodels/CameraControlViewModel.h
        src/viewmodels/CameraControlViewModel.cpp
        include/viewmodels/DeviceListModel.h
        src/viewmodels/DeviceListModel.cpp
        include/viewmodels/FrameView.h
        src/viewmodels/FrameView.cpp
        include/viewmodels/SettingsViewModel.h
        src/viewmodels/SettingsViewModel.cpp
        include/viewmodels/VirtualCamViewModel.h
        src/viewmodels/VirtualCamViewModel.cpp
        include/updater/UpdateChecker.h
        src/updater/UpdateChecker.cpp
)

# ── External opaque resource bundle: bin/viewcam.rcc ──────────
# Built with `rcc --binary`, registered at runtime via QResource. Layout:
#   /qt/qml/ViewCam/Studio/   qmldir + every *.qml + fonts/images/icons
#   /i18n/                    compiled *.qm translation catalogs
# Nothing below is compiled into the executable.
include(ViewCamBundle)
viewcam_build_bundle()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    # qmltyperegistrations includes QML_ELEMENT headers by basename
    ${CMAKE_SOURCE_DIR}/include/viewmodels
    ${CMAKE_SOURCE_DIR}/include/updater
    ${CMAKE_CURRENT_BINARY_DIR}/generated   # ViewCamConfig.h
    ${CMAKE_CURRENT_BINARY_DIR}
    # MFVirtualCamManager.h lives next to its .cpp (not in public include/)
    $<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}/src/virtualcam/windows>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::Network
    Qt6::Svg
)

if(UNIX AND SPDLOG_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SPDLOG_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SPDLOG_LIBRARIES})
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
endif()

# ── QML hot-reload (Debug only) ───────────────────────────────
# VIEWCAM_HOT_RELOAD activates QmlDevMode in QmlDevMode.cpp. The source
# qml/ directory is baked in as an absolute path at configure time so no
# env var is needed — just build Debug and live edits reload instantly.
# To use a different source dir at runtime, set VIEWCAM_QML_DIR env.
get_property(_vc_multiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_vc_multiConfig)
    # Multi-config generators (VS, Xcode): CMAKE_BUILD_TYPE is meaningless here.
    # Bake the path unconditionally (used only inside #ifdef VIEWCAM_HOT_RELOAD in
    # QmlDevMode.cpp, so it has zero overhead in Release). Gate the reload flag itself
    # per-config via a generator expression.
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VIEWCAM_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY COMPILE_DEFINITIONS
        $<$<CONFIG:Debug>:VIEWCAM_HOT_RELOAD>)
    message(STATUS "QML hot-reload: per-config (Debug=ENABLED, Release=disabled)")
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VIEWCAM_HOT_RELOAD
        VIEWCAM_QML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
    message(STATUS "QML hot-reload: ENABLED (Debug build, source: ${CMAKE_CURRENT_SOURCE_DIR}/qml)")
else()
    message(STATUS "QML hot-reload: disabled (${CMAKE_BUILD_TYPE} build)")
endif()

# GPU backends: compile-define gate + runtime lib per backend. All CUDA/Vulkan
# code is behind #ifdef VIEWCAM_HAVE_CUDA / VIEWCAM_HAVE_VULKAN, so these are the
# only places the dependencies appear.
if(VIEWCAM_HAVE_CUDA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VIEWCAM_HAVE_CUDA)
    target_link_libraries(${PROJECT_NAME} PRIVATE CUDA::cudart)
endif()
if(VIEWCAM_HAVE_VULKAN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VIEWCAM_HAVE_VULKAN)
    target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE advapi32 shell32 mfplat mfuuid ole32)
endif()

# ── Deploy (portable bin/ directory) ──────────────────────────
# Shipped layout: bin/ViewCam (exe) + bin/viewcam.rcc (opaque UI+i18n blob)
# + Qt runtime (lib/, plugins/, qml/ for the Qt-provided imports only).
set(DEPLOY_DIR "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "Deployment output directory (overridable via -DDEPLOY_DIR=...)")

# Copy the external resource bundle next to the executable.
# Attached to BOTH the rcc target and the binary so it runs whenever the rcc
# is regenerated (QML-only changes don't relink the C++ binary).
add_custom_command(TARGET viewcam_rcc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VIEWCAM_RCC_FILE}" "${DEPLOY_DIR}/viewcam.rcc"
    COMMENT "Deploying viewcam.rcc to bin/ (rcc target)")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VIEWCAM_RCC_FILE}" "${DEPLOY_DIR}/viewcam.rcc"
    COMMENT "Deploying viewcam.rcc next to the executable")

if(WIN32)
    # ── Windows Deployment ────────────────────────────────────
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DEPLOY_DIR}"
    )

    get_target_property(_qt_qmake_loc Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake_loc}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt6 windeployqt
        HINTS "${_qt_bin_dir}"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --qmldir "${CMAKE_SOURCE_DIR}/qml" "$<TARGET_FILE:${PROJECT_NAME}>"
            WORKING_DIRECTORY "${DEPLOY_DIR}"
            COMMENT "Running windeployqt to deploy Qt DLLs + QML imports..."
        )
    else()
        message(WARNING "windeployqt not found — Qt DLLs will NOT be copied automatically.")
    endif()

else()
    # ── Linux Deployment ──────────────────────────────────────
    set(DEPLOY_LIB_DIR "${DEPLOY_DIR}/lib")
    set(DEPLOY_PLUGINS_DIR "${DEPLOY_DIR}/plugins/platforms")
    set(DEPLOY_QML_DIR "${DEPLOY_DIR}/qml")

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
    find_path(QT_QML_DIR NAMES QtQuick
        PATHS
            "${QT_LIB_DIR}/../qml"
            "${QT_LIB_DIR}/qt6/qml"
            "${QT_LIB_DIR}/qt/qml"
        NO_DEFAULT_PATH
    )

    # Resolve spdlog/fmt library paths
    pkg_get_variable(SPDLOG_LIBDIR spdlog libdir)

    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_LIB_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_PLUGINS_DIR}"

        # Copy Qt shared libraries (QML/Quick stack, no Widgets)
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_LIB_DIR}/libQt6Core.so.6"
            "${QT_LIB_DIR}/libQt6Gui.so.6"
            "${QT_LIB_DIR}/libQt6Qml.so.6"
            "${QT_LIB_DIR}/libQt6QmlMeta.so.6"
            "${QT_LIB_DIR}/libQt6QmlModels.so.6"
            "${QT_LIB_DIR}/libQt6QmlWorkerScript.so.6"
            "${QT_LIB_DIR}/libQt6Quick.so.6"
            "${QT_LIB_DIR}/libQt6QuickEffects.so.6"
            "${QT_LIB_DIR}/libQt6OpenGL.so.6"
            "${QT_LIB_DIR}/libQt6Network.so.6"
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

    # Copy QML import dirs needed at runtime (QtQuick base + Effects, QtQml)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_QML_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_QML_DIR}/QtQml" "${DEPLOY_QML_DIR}/QtQml"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QT_QML_DIR}/QtQuick" "${DEPLOY_QML_DIR}/QtQuick"
        COMMENT "Deploying QML imports to bin/qml/"
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

    # Copy SVG icon plugin (needed for QIcon/Image to render SVG)
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
export QML_IMPORT_PATH="$SCRIPT_DIR/qml"
exec "$SCRIPT_DIR/ViewCam" "$@"
]=])
    file(CHMOD "${DEPLOY_DIR}/viewcam.sh"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE
    )
endif()

# Translations are compiled (.ts -> .qm) and bundled into viewcam.rcc by
# cmake/ViewCamBundle.cmake — no loose .qm are shipped. Refresh catalogs with:
#   cmake --build build --target viewcam_lupdate

# ── DirectShow Virtual Camera Filter DLL (Windows only) ──────
if(WIN32)
    add_library(ViewCamFilter SHARED
        src/virtualcam/windows/filter/VcamFilter.cpp
        src/virtualcam/windows/filter/VcamFilter.def
    )

    target_include_directories(ViewCamFilter PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/virtualcam/windows/filter
    )

    target_link_libraries(ViewCamFilter PRIVATE
        strmiids
        ole32
        oleaut32
        uuid
        advapi32
    )

    set_target_properties(ViewCamFilter PROPERTIES
        PREFIX ""                          # GCC adds "lib" by default; strip it so the DLL is ViewCamFilter.dll
        RUNTIME_OUTPUT_DIRECTORY "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DEPLOY_DIR}"
    )
endif()

# ── MF Virtual Camera Source DLL (Windows 11 virtual camera API) ──────────────
if(WIN32)
    add_library(ViewCamMFSource SHARED
        src/virtualcam/windows/VcamMFSource.cpp
        src/virtualcam/windows/VcamMFSource.def
    )

    target_include_directories(ViewCamMFSource PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(ViewCamMFSource PRIVATE
        mfplat
        mfuuid
        ole32
        oleaut32
        uuid
        advapi32
    )

    # Static-link libgcc and libstdc++ so the DLL has no MSYS2 runtime
    # dependencies — the Windows Frame Server broker loads this DLL in a
    # separate process and won't find MSYS2 DLLs in its search path.
    # GCC only — MSVC uses /MT[d] via CMAKE_MSVC_RUNTIME_LIBRARY.
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_link_options(ViewCamMFSource PRIVATE -static-libgcc -static-libstdc++)
    endif()

    set_target_properties(ViewCamMFSource PROPERTIES
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${DEPLOY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${DEPLOY_DIR}"
    )
endif()

# ── Connectivity self-test (Linux) ───────────────────────────
# Single-process end-to-end proof of the discovery/stream path. Reuses the
# real Desktop network + virtualcam sources. Build+run:
#   cmake --build build --target vc_proto_selftest && build/vc_proto_selftest
if(LINUX)
    add_executable(vc_proto_selftest
        tools/proto_selftest.cpp
        src/network/DeviceDiscovery.cpp   include/network/DeviceDiscovery.h
        src/network/StreamReceiver.cpp    include/network/StreamReceiver.h
        src/network/FrameDecoder.cpp      include/network/FrameDecoder.h
        src/virtualcam/linux/V4L2LoopbackWriter.cpp include/virtualcam/V4L2LoopbackWriter.h
        src/core/Logger.cpp
    )
    target_include_directories(vc_proto_selftest PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/generated)
    target_link_libraries(vc_proto_selftest PRIVATE
        Qt6::Core Qt6::Gui Qt6::Network)
    if(UNIX AND SPDLOG_FOUND)
        target_include_directories(vc_proto_selftest PRIVATE ${SPDLOG_INCLUDE_DIRS})
        target_link_libraries(vc_proto_selftest PRIVATE ${SPDLOG_LIBRARIES})
    else()
        target_link_libraries(vc_proto_selftest PRIVATE spdlog::spdlog)
    endif()
endif()

# ── Install ────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
