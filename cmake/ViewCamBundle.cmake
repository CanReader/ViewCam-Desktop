# ViewCamBundle.cmake — build the external opaque resource blob (viewcam.rcc).
#
# Produces ${CMAKE_BINARY_DIR}/viewcam.rcc containing the whole UI (qml +
# qmldir + fonts/images/icons) and compiled translations, none of which is
# embedded in the executable. The app registers it at startup with
# QResource::registerResource(). The blob is rcc-compressed (opaque binary);
# an optional XOR scramble is described in the brief but left disabled.
#
# Reads from the caller scope: VIEWCAM_QML_FILES, VIEWCAM_ASSET_FILES,
# VIEWCAM_QML_SINGLETONS, PROJECT_NAME.

set(VIEWCAM_RCC_URI_PATH "qt/qml/ViewCam/Studio")

function(viewcam_build_bundle)
    set(_gen "${CMAKE_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY "${_gen}")

    # ── 1. Generate qmldir from the component list ────────────────
    set(_qmldir "module ViewCam.Studio\nprefer :/${VIEWCAM_RCC_URI_PATH}/\n")
    foreach(_f ${VIEWCAM_QML_FILES})
        get_filename_component(_name "${_f}" NAME_WE)
        if("${_name}" IN_LIST VIEWCAM_QML_SINGLETONS)
            string(APPEND _qmldir "singleton ${_name} 1.0 ${_f}\n")
        else()
            string(APPEND _qmldir "${_name} 1.0 ${_f}\n")
        endif()
    endforeach()
    string(APPEND _qmldir "depends QtQuick\n")
    file(WRITE "${_gen}/qmldir" "${_qmldir}")

    # ── 2. Compile translations (.ts -> .qm) into the bundle ──────
    file(GLOB _ts_files "${CMAKE_SOURCE_DIR}/translations/*.ts")
    set(_qm_files "")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/i18n")
    foreach(_ts ${_ts_files})
        get_filename_component(_ts_name "${_ts}" NAME_WE)
        set(_qm "${CMAKE_BINARY_DIR}/i18n/${_ts_name}.qm")
        add_custom_command(
            OUTPUT "${_qm}"
            COMMAND Qt6::lrelease "${_ts}" -qm "${_qm}"
            DEPENDS "${_ts}"
            COMMENT "lrelease ${_ts_name}.ts -> i18n/${_ts_name}.qm"
            VERBATIM)
        list(APPEND _qm_files "${_qm}")
    endforeach()

    # ── 3. Generate the .qrc describing the blob ──────────────────
    set(_qrc "<RCC>\n  <qresource prefix=\"/${VIEWCAM_RCC_URI_PATH}/\">\n")
    string(APPEND _qrc "    <file alias=\"qmldir\">${_gen}/qmldir</file>\n")
    foreach(_f ${VIEWCAM_QML_FILES})
        string(APPEND _qrc "    <file alias=\"${_f}\">${CMAKE_SOURCE_DIR}/${_f}</file>\n")
    endforeach()
    foreach(_f ${VIEWCAM_ASSET_FILES})
        string(APPEND _qrc "    <file alias=\"${_f}\">${CMAKE_SOURCE_DIR}/${_f}</file>\n")
    endforeach()
    string(APPEND _qrc "  </qresource>\n  <qresource prefix=\"/i18n/\">\n")
    foreach(_qm ${_qm_files})
        get_filename_component(_qm_name "${_qm}" NAME)
        string(APPEND _qrc "    <file alias=\"${_qm_name}\">${_qm}</file>\n")
    endforeach()
    string(APPEND _qrc "  </qresource>\n</RCC>\n")
    set(_qrc_path "${_gen}/viewcam.qrc")
    file(WRITE "${_qrc_path}" "${_qrc}")

    # ── 4. Compile the blob with rcc --binary ─────────────────────
    # Output is a single opaque, rcc-compressed binary. (Optional XOR scramble
    # is documented in the brief but intentionally not enabled — see notes.)
    set(_final_rcc "${CMAKE_BINARY_DIR}/viewcam.rcc")
    qt6_add_binary_resources(viewcam_rcc "${_qrc_path}"
        DESTINATION "${_final_rcc}")
    # qt6_add_binary_resources parses the qrc at configure time but does not
    # know the .qm are generated; make the dependency explicit.
    add_custom_target(viewcam_qm DEPENDS ${_qm_files})
    add_dependencies(viewcam_rcc viewcam_qm)
    add_dependencies(${PROJECT_NAME} viewcam_rcc)
    set(VIEWCAM_RCC_FILE "${_final_rcc}" PARENT_SCOPE)

    # ── 5. Manual i18n refresh: `cmake --build build --target viewcam_lupdate`
    add_custom_target(viewcam_lupdate
        COMMAND Qt6::lupdate
            ${CMAKE_SOURCE_DIR}/qml ${CMAKE_SOURCE_DIR}/src
            -ts ${_ts_files}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Updating .ts catalogs from qml/ and src/"
        VERBATIM)
endfunction()
