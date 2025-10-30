cmake_minimum_required(VERSION 3.16)
project(fcitx5-schnelle-umlaute VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM REQUIRED 1.0.0)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(Fcitx5Core REQUIRED)
find_package(Gettext REQUIRED)
find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend TestIM)

include(GNUInstallDirs)

# Sources
set(SCHNELLE_UMLAUTE_SOURCES
    src/schnelle-umlaute.cpp
)

# Build addon shared library
add_library(schnelle-umlaute MODULE ${SCHNELLE_UMLAUTE_SOURCES})
target_link_libraries(schnelle-umlaute Fcitx5::Core)
set_target_properties(schnelle-umlaute PROPERTIES PREFIX "")

# Install addon
install(TARGETS schnelle-umlaute DESTINATION "${CMAKE_INSTALL_LIBDIR}/fcitx5")

# Install addon configuration
fcitx5_translate_desktop_file(org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml.in
                               org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml XML)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.fcitx.Fcitx5.Addon.SchnelleUmlaute.metainfo.xml"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/addon")

# Install configuration files
install(FILES data/schnelle-umlaute.conf
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/addon")

# Install configuration descriptor
install(FILES data/schnelle-umlaute.conf.in
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/addon")

# Install input method configuration
install(FILES data/schnelle-umlaute-inputmethod.conf
        DESTINATION "${CMAKE_INSTALL_DATADIR}/fcitx5/inputmethod"
        RENAME schnelle-umlaute.conf)
