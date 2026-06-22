# Package.cmake — produce the full-folder update archive from the deployed bin/.
#
# Run in CMake script mode (cmake -P) by the `viewcam_package` target:
#   cmake -DDEPLOY_DIR=<bin> -DDIST_DIR=<dist> -DVERSION=<x.y.z>
#         -DPLATFORM=<linux-x86_64|win-x64> -P cmake/Package.cmake
#
# Output: ${DIST_DIR}/ViewCam-${VERSION}-${PLATFORM}.zip
# Archive ROOT entries = the version-dir contents the client extracts:
#   ViewCam(.exe) + viewcam.rcc + vc-updater(.exe) + lib/ plugins/ qml/ (+ viewcam.sh on Linux).
#
# The dev-only `dev-imports/` shim (absolute source-tree QML paths) is EXCLUDED:
# production QML is served from viewcam.rcc, and those paths only exist on the
# build machine — shipping them would make the build non-hermetic.

if(NOT DEPLOY_DIR OR NOT DIST_DIR OR NOT VERSION OR NOT PLATFORM)
    message(FATAL_ERROR "Package.cmake requires DEPLOY_DIR, DIST_DIR, VERSION, PLATFORM")
endif()

if(NOT EXISTS "${DEPLOY_DIR}/ViewCam" AND NOT EXISTS "${DEPLOY_DIR}/ViewCam.exe")
    message(FATAL_ERROR
        "No deployed ViewCam in '${DEPLOY_DIR}'. Build the ViewCam + vc-updater targets first.")
endif()

set(_zip_name "ViewCam-${VERSION}-${PLATFORM}.zip")
set(_out_zip  "${DIST_DIR}/${_zip_name}")

# Stage a clean copy of bin/ so we can drop dev-only artifacts without touching
# the live deploy. cmake -E copy_directory preserves the executable bits.
set(_stage "${DIST_DIR}/.stage-${PLATFORM}")
file(REMOVE_RECURSE "${_stage}")
file(MAKE_DIRECTORY "${_stage}")
file(COPY "${DEPLOY_DIR}/" DESTINATION "${_stage}")

# Drop dev-only / non-shippable entries from the archive root.
file(REMOVE_RECURSE "${_stage}/dev-imports")

file(MAKE_DIRECTORY "${DIST_DIR}")
file(REMOVE "${_out_zip}")

# Zip from WITHIN the stage dir so archive root == bin/ contents (no leading path).
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${_out_zip}" --format=zip .
    WORKING_DIRECTORY "${_stage}"
    RESULT_VARIABLE _rc
)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cmake -E tar failed (rc=${_rc}) building ${_out_zip}")
endif()

file(REMOVE_RECURSE "${_stage}")
file(SIZE "${_out_zip}" _zsize)
message(STATUS "Packaged ${_zip_name} (${_zsize} bytes) -> ${_out_zip}")
message(STATUS "Sign + manifest it with:  scripts/release.sh ${VERSION}")
