find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2)

qt_policy(SET QTP0001 NEW)
qt_policy(SET QTP0004 NEW)

set(EDITOR_SRCS
    main.cpp
    MappingListModel.cpp
    SettingsModel.cpp
)

qt_add_executable(schnelle-umlaute-editor ${EDITOR_SRCS})

set_target_properties(schnelle-umlaute-editor PROPERTIES
    AUTOMOC TRUE
)

set_source_files_properties(Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_qml_module(schnelle-umlaute-editor
    URI SchnelleUmlaute
    VERSION 1.0
    QML_FILES
        Main.qml
        Mappings.qml
        Settings.qml
        Theme.qml
        components/Header.qml
        components/AddMappingCard.qml
        components/MappingRow.qml
        components/EmptyState.qml
        components/Footer.qml
        components/SettingsCard.qml
        components/LabeledSlider.qml
        components/LabeledSwitch.qml
        components/CustomLeaderRow.qml
        components/AppListEditor.qml
        components/ConfirmDialog.qml
        components/PositionPicker.qml
    RESOURCES
        assets/schnelle-umlaute-icon.svg
)

target_link_libraries(schnelle-umlaute-editor PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
)

target_compile_options(schnelle-umlaute-editor PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS schnelle-umlaute-editor
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES schnelle-umlaute-editor.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

install(FILES ${CMAKE_SOURCE_DIR}/../docs/assets/schnelle-umlaute-icon.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME schnelle-umlaute-editor.svg)
