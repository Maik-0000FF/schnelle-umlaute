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
target_link_libraries(schnelle-umlaute-config-editor
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Widgets
    Fcitx5Qt${QT_MAJOR_VERSION}::WidgetsAddons
)

install(TARGETS schnelle-umlaute-config-editor
        DESTINATION "${FCITX_INSTALL_ADDONDIR}/qt${QT_MAJOR_VERSION}")
