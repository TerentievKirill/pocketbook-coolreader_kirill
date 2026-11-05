set(CR3_RELEASE_VERSION "1.0.4" CACHE STRING "Stable OTA version (SemVer)")
if(NOT CR3_RELEASE_VERSION MATCHES "^(0|[1-9][0-9]*)[.](0|[1-9][0-9]*)[.](0|[1-9][0-9]*)$")
    message(FATAL_ERROR "CR3_RELEASE_VERSION must be MAJOR.MINOR.PATCH")
endif()
add_definitions(-DCR3_RELEASE_VERSION="${CR3_RELEASE_VERSION}")
# PocketBook UI. Other historical GUI source files are absent from this tree.
set(DEVICE_NAME pb360 CACHE STRING "PocketBook keymap and skin source directory")
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/generate-device-profiles.py
    ${CMAKE_SOURCE_DIR}/config/pocketbook-devices.json ${CMAKE_CURRENT_BINARY_DIR}/pb_device_data.h
    RESULT_VARIABLE device_profile_result)
if(NOT device_profile_result EQUAL 0)
    message(FATAL_ERROR "Device profile generation failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/config/pocketbook-devices.json ${CMAKE_SOURCE_DIR}/tools/generate-device-profiles.py)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
option(CR3_SHARED_PACKAGE "Experimental shared PocketBook package; USB updates only" OFF)
if(CR3_SHARED_PACKAGE)
    add_definitions(-DCR3_SHARED_PACKAGE=1)
endif()

set(KWWidgets_CMAKE_DIR ${CMAKE_SOURCE_DIR}/tools/)
include(${KWWidgets_CMAKE_DIR}FindGettext.cmake)
include(${KWWidgets_CMAKE_DIR}KWWidgetsInternationalizationMacros.cmake)
set(LANGUAGES bg ca cs de es et fr he hu it lt nl pl pt ro ru tr uk zh)

set(CR3_SOURCES
    src/bgfit.cpp
    src/citedlg.cpp
    src/fsmenu.cpp
    src/mainwnd.cpp
    src/recentdlg.cpp
    src/selnavig.cpp
    src/tocdlg.cpp
    src/bmkdlg.cpp
    src/cr3_def_cover.cpp
    src/cr3main.cpp
    src/dictdlg.cpp
    src/linksdlg.cpp
    src/numedit.cpp
    src/scrkbd.cpp
    src/settings.cpp
    src/viewdlg.cpp
    src/cr3pocketbook.cpp
)
if(POCKETBOOK_PRO)
    list(APPEND CR3_SOURCES src/web.cpp src/web_transfer.cpp src/ota_update.cpp src/pb_toc.cpp)
    add_definitions(-DPOCKETBOOK_PRO=1)
    if(POCKETBOOK_PRO_PRO2)
        add_definitions(-DPOCKETBOOK_PRO_PRO2=1)
    elseif(POCKETBOOK_PRO_FW5)
        # FW6 also uses these compatible InkView interfaces.
        add_definitions(-DPOCKETBOOK_PRO_FW5=1)
    endif()
endif()
if(BACKGROUND_CACHE_FILE_CREATION)
    add_definitions(-DBACKGROUND_CACHE_FILE_CREATION=1)
endif()

KWWidgets_CREATE_GETTEXT_TARGETS(
    DOMAIN_NAME "cr3"
    LOCALE_LIST "${LANGUAGES}"
    SOURCES "${CR3_SOURCES}"
    PO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/po
    CREATE_POT_TARGET 1
    CREATE_PO_TARGET 1
    MO_BUILD_DIR ${CMAKE_BINARY_DIR}${CMAKE_DATA_DIR}/i18n
    ADD_MO_TARGET_TO_ALL 1
)

find_library(INKVIEW_LIB inkview)
if(NOT INKVIEW_LIB)
    message(FATAL_ERROR "libinkview not found; use the PocketBook SDK toolchain")
endif()
set(EXTRA_LIBS ${INKVIEW_LIB})
if(ENABLE_PB_DB_STATE)
    find_library(BOOKSTATE_LIB bookstate)
    if(BOOKSTATE_LIB)
        add_definitions(-DPB_DB_STATE_SUPPORTED=1)
        list(APPEND EXTRA_LIBS dl)
    endif()
endif()


find_program(ZIP_EXECUTABLE zip)
if(NOT ZIP_EXECUTABLE)
    message(FATAL_ERROR "Unable to find 'zip' executable")
endif()
set(CR3_SKINS default pb62x pb626fw5 pb631fw5)
foreach(skin ${CR3_SKINS})
    file(GLOB skin_files ${CMAKE_CURRENT_SOURCE_DIR}/data/devices/${DEVICE_NAME}/skins/${skin}/*)
    set(skin_archive ${CMAKE_CURRENT_BINARY_DIR}/${skin}.cr3skin)
    add_custom_command(OUTPUT ${skin_archive}
        COMMAND ${CMAKE_COMMAND} -E remove -f ${skin_archive}
        COMMAND ${ZIP_EXECUTABLE} -j ${skin_archive} ${skin_files}
        DEPENDS ${skin_files}
        VERBATIM)
    add_custom_target(skin_${skin} ALL DEPENDS ${skin_archive})
    install(FILES ${skin_archive} DESTINATION system/share/cr3/skins)
endforeach()

add_executable(cr3-pb.app ${CR3_SOURCES})
add_dependencies(cr3-pb.app jpeg_external curl_external)
target_link_libraries(cr3-pb.app crengine tinydict ${EXTRA_LIBS} ${STD_LIBS})
if(POCKETBOOK_FW6)
    target_link_libraries(cr3-pb.app m)
    # InkView must bind its firmware libraries rather than our private copies.
    set_property(TARGET cr3-pb.app APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--exclude-libs,ALL")
endif()
# Packaging alone strips a copy; keep the link output and its symbols intact.
install(TARGETS cr3-pb.app RUNTIME DESTINATION system/bin)
set(CR3_DEFAULTS_FILE "${CMAKE_SOURCE_DIR}/data/touch-hd3/cr3-defaults.ini" CACHE FILEPATH "Packaged reading defaults")
install(FILES ${CR3_DEFAULTS_FILE} DESTINATION system/share/cr3)
install(FILES data/epub.css data/fb2.css data/htm.css data/rtf.css
    data/txt.css data/chm.css data/doc.css data/dict.css DESTINATION system/share/cr3)
install(DIRECTORY data/hyph/ DESTINATION system/share/cr3/hyph
    FILES_MATCHING PATTERN "*.pdb" PATTERN "*.pattern")
install(DIRECTORY data/chn/ DESTINATION system/share/cr3/chn FILES_MATCHING PATTERN "*.dic")
install(DIRECTORY data/devices/${DEVICE_NAME}/keymaps/ DESTINATION system/share/cr3/keymaps)
install(DIRECTORY data/manual/ DESTINATION system/share/cr3/manual)
foreach(language ${LANGUAGES})
    install(FILES ${CMAKE_BINARY_DIR}/i18n/${language}/LC_MESSAGES/cr3.mo
        DESTINATION system/share/cr3/i18n RENAME ${language}.mo)
endforeach()
