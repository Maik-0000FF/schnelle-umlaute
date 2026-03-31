cmake_minimum_required(VERSION 3.16)
project(fcitx5-schnelle-umlaute VERSION 0.1.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED 1.0.0)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# Install addon files to fcitx5's own directories (e.g. /usr/lib/fcitx5 on
# Arch, /usr/lib/x86_64-linux-gnu/fcitx5 on Debian, /usr/local/lib/fcitx5
# when fcitx5 was built from source).  Must be set before find_package so
# Fcitx5UtilsConfig.cmake picks up the correct paths.
set(FCITX_INSTALL_USE_FCITX_SYS_PATHS ON CACHE BOOL
    "Install to fcitx5's system directories instead of CMAKE_INSTALL_PREFIX")

find_package(Fcitx5Core REQUIRED)
find_package(Gettext REQUIRED)

set(QT_MAJOR_VERSION 6)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(Fcitx5Qt${QT_MAJOR_VERSION}WidgetsAddons REQUIRED)

include(GNUInstallDirs)

# Sources
set(SCHNELLE_UMLAUTE_SOURCES
    src/schnelle-umlaute.cpp
)

# Build addon shared library
add_library(schnelle-umlaute MODULE ${SCHNELLE_UMLAUTE_SOURCES})
target_link_libraries(schnelle-umlaute Fcitx5::Core)
target_compile_options(schnelle-umlaute PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(schnelle-umlaute PROPERTIES PREFIX "")

# Install addon library to fcitx5's own addon directory.
# FCITX_INSTALL_ADDONDIR is provided by Fcitx5Utils and always points to
# the correct path (e.g. /usr/lib/fcitx5 on Arch, /usr/lib/x86_64-linux-gnu/fcitx5 on Debian).
install(TARGETS schnelle-umlaute DESTINATION "${FCITX_INSTALL_ADDONDIR}")

# Install addon configuration
fcitx5_translate_desktop_file(org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml.in
                               org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml XML)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

# Install configuration files
install(FILES data/schnelle-umlaute.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

# Install configuration descriptor
install(FILES data/schnelle-umlaute.conf.in
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")

# Install input method configuration
install(FILES data/schnelle-umlaute-inputmethod.conf
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/inputmethod"
        RENAME schnelle-umlaute.conf)

# Config editor Qt plugin
add_subdirectory(config-editor)
