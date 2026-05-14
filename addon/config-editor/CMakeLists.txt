set(MAPPING_EDITOR_SRCS
    main.cpp
    editor.cpp
    model.cpp
)

add_library(schnelle-umlaute-config-editor MODULE ${MAPPING_EDITOR_SRCS})
set_target_properties(schnelle-umlaute-config-editor PROPERTIES
    AUTOMOC TRUE
    AUTOUIC TRUE
    AUTOUIC_OPTIONS "-tr=fcitx::tr2fcitx;--include=fcitxqti18nhelper.h"
)
target_compile_options(schnelle-umlaute-config-editor PRIVATE -Wall -Wextra -Wpedantic)
# QT_NO_KEYWORDS forces use of Q_SIGNALS / Q_SLOTS / Q_EMIT / Q_FOREACH in
# our own Qt code (the editor already uses Q_SLOTS exclusively, so no source
# change is needed). The reason this matters is interoperability with fcitx5
# headers: a few of them — connectableobject.h (method `emit`),
# inputcontextmanager.h and focusgroup.h (method `foreach`) — use these names
# as ordinary identifiers. Pulling any of them into a Qt TU under the
# default macro set silently destroys those declarations through preprocessor
# substitution. With QT_NO_KEYWORDS the macros stay namespaced, so future
# editor refactors that need additional fcitx headers don't run into a
# preprocessor wall.
target_compile_definitions(schnelle-umlaute-config-editor PRIVATE QT_NO_KEYWORDS)
target_link_libraries(schnelle-umlaute-config-editor
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Widgets
    Fcitx5Qt${QT_MAJOR_VERSION}::WidgetsAddons
)

install(TARGETS schnelle-umlaute-config-editor
        DESTINATION "${FCITX_INSTALL_ADDONDIR}/qt${QT_MAJOR_VERSION}")
