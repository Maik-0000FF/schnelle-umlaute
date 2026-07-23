# Shared theme-palette QML module. A tiny static QML library holding the single
# Palettes singleton (Palettes.qml), imported by BOTH the editor
# (SchnelleUmlaute) and the overlay (SchnelleUmlauteOverlay) modules so a theme
# is defined in one place instead of duplicated across the two.
qt_add_library(schnelle-umlaute-palette STATIC)

# QML type registration for the module runs metatype generation, which needs
# AUTOMOC even though this module is pure QML (no C++ types of its own).
set_target_properties(schnelle-umlaute-palette PROPERTIES AUTOMOC ON)

# Mark Palettes.qml as a QML singleton in the generated qmldir. `pragma
# Singleton` alone is honoured within a module, but a module that IMPORTS this
# one only sees Palettes as a singleton instance (with its get/has/overlayOf
# functions) when the qmldir declares it `singleton`, which this property drives.
set_source_files_properties(Palettes.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(schnelle-umlaute-palette
    URI SchnelleUmlautePalette
    VERSION 1.0
    # Pin the resource prefix to /qt/qml on every Qt version, same reason as the
    # editor and overlay modules (see addon/editor/CMakeLists.txt): the Qt 6.4
    # default differs and would leave the imported singleton unresolved.
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Palettes.qml
)
