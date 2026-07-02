find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Qml Quick QuickControls2)

# qt_policy was introduced in Qt 6.5. Skip cleanly on older Qt (e.g. Qt
# 6.4 on Ubuntu 24.04) so the build still works; the default resource
# prefix there is good enough.
if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

set(EDITOR_SRCS
    main.cpp
    EnvSetup.cpp
    KeyComboUtil.cpp
    MappingListModel.cpp
    OverlayDBusClient.cpp
    ProfileListModel.cpp
    SettingsModel.cpp
    SingleInstance.cpp
    keycombo.cpp
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
    # Pin the QML resource prefix to /qt/qml on every Qt version. This is the
    # Qt 6.5+ default (via QTP0001), but on Qt 6.4 (Ubuntu 24.04 / Linux Mint
    # 22) the default is /<URI>/, so the module files land at a different path
    # than where `import SchnelleUmlaute` and the hard-coded asset URLs look
    # for them. That mismatch left the Theme singleton unresolved on 6.4: every
    # Theme.* binding was undefined and the controls collapsed onto one
    # position. Forcing the prefix keeps the layout identical across distros.
    RESOURCE_PREFIX "/qt/qml"
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
        components/LabeledRangeSlider.qml
        components/LabeledSwitch.qml
        components/CustomLeaderRow.qml
        components/CycleShortcutRow.qml
        components/AppListEditor.qml
        components/ConfirmDialog.qml
        components/DropdownIndicator.qml
        components/KeyCaptureField.qml
        components/LibrarySelector.qml
        components/PositionPicker.qml
        components/ProfileSelector.qml
        components/ThemedComboBox.qml
        components/ThemedTextField.qml
        components/ThemedToolTip.qml
    RESOURCES
        assets/schnelle-umlaute-icon.png
)

target_link_libraries(schnelle-umlaute-editor PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    # Fcitx5::Core provides fcitx::Key, so ProfileListModel's duplicate-shortcut
    # check normalizes combos exactly like the engine's runtime matcher.
    Fcitx5::Core
)

target_compile_options(schnelle-umlaute-editor PRIVATE -Wall -Wextra -Wpedantic)
# QT_NO_KEYWORDS forces use of Q_SIGNALS / Q_SLOTS / Q_EMIT / Q_FOREACH in
# our own Qt code (MappingListModel / SettingsModel already use these
# exclusively, so no source change is needed). The reason this matters is
# interoperability with fcitx5 if the editor ever pulls in fcitx headers
# directly: several fcitx Util headers declare methods named `emit` /
# `foreach` as ordinary identifiers. Pulling any of them into a Qt TU
# under the default macro set silently destroys those declarations through
# preprocessor substitution. With QT_NO_KEYWORDS the macros stay namespaced,
# so future editor refactors don't run into a preprocessor wall.
target_compile_definitions(schnelle-umlaute-editor PRIVATE QT_NO_KEYWORDS)

# Absolute install path of the bundled presets, baked in as the last-resort
# fallback for preset_paths.h::presetsDir() when neither the binary-relative
# path nor XDG resolves (e.g. an unusual prefix). The primary resolution stays
# runtime (binary-relative + XDG), so this only needs to be correct, not the
# only source.
target_compile_definitions(schnelle-umlaute-editor PRIVATE
    SCHNELLE_UMLAUTE_PRESETS_DIR="${CMAKE_INSTALL_FULL_DATADIR}/schnelle-umlaute/presets")

install(TARGETS schnelle-umlaute-editor
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES schnelle-umlaute-editor.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")

install(FILES ${CMAKE_SOURCE_DIR}/../docs/assets/schnelle-umlaute-icon.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME schnelle-umlaute-editor.svg)

# Bundled preset profiles, read at runtime by the editor's "add from library"
# picker (see preset_paths.h). Installed next to the editor's other share files
# so the binary-relative resolution (<prefix>/bin -> <prefix>/share/...) finds
# them on every prefix-based layout, including the Nix store.
install(DIRECTORY ${CMAKE_SOURCE_DIR}/presets/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/schnelle-umlaute/presets"
        FILES_MATCHING PATTERN "*.txt")
