find_package(Qt6 REQUIRED COMPONENTS Core DBus Gui Qml Quick)

# LayerShellQt is Wayland-only and not packaged on every distro. Skip the
# overlay daemon cleanly if the dev package is missing — the addon and the
# editor don't depend on it, so CI and builds on bare systems still succeed.
find_package(LayerShellQt QUIET)
if(NOT LayerShellQt_FOUND)
    message(STATUS
        "LayerShellQt not found — skipping schnelle-umlaute-overlay. "
        "Install layer-shell-qt (Arch), liblayershellqtinterface-dev "
        "(Debian/Ubuntu) or layer-shell-qt-devel (Fedora/openSUSE) to "
        "enable the cycle overlay.")
    return()
endif()

if(COMMAND qt_policy)
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 NEW)
endif()

set(OVERLAY_SRCS
    main.cpp
    OverlayController.cpp
)

qt_add_executable(schnelle-umlaute-overlay ${OVERLAY_SRCS})

set_target_properties(schnelle-umlaute-overlay PROPERTIES
    AUTOMOC TRUE
)

qt_add_qml_module(schnelle-umlaute-overlay
    URI SchnelleUmlauteOverlay
    VERSION 1.0
    # Pin the resource prefix to /qt/qml on every Qt version, same as the
    # editor module (see addon/editor/CMakeLists.txt). Overlay.qml is a single
    # self-contained file with no module import, so the old-prefix mismatch did
    # not break it on Qt 6.4, but keeping both modules on one prefix removes the
    # latent inconsistency and matches the Qt 6.5+ default.
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        Overlay.qml
)

target_link_libraries(schnelle-umlaute-overlay PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    LayerShellQt::Interface
)

target_compile_options(schnelle-umlaute-overlay PRIVATE -Wall -Wextra -Wpedantic)
# QT_NO_KEYWORDS forces use of Q_SIGNALS / Q_SLOTS / Q_EMIT / Q_FOREACH in
# OverlayController (already the case today). Same reasoning as the editor
# target: keeps the Qt macros namespaced so a future include of an fcitx5
# header declaring methods named `emit` / `foreach` is not preprocessor-
# corrupted into Qt's macro forms.
target_compile_definitions(schnelle-umlaute-overlay PRIVATE QT_NO_KEYWORDS)

# Screen-selection API differs across LayerShellQt versions:
#   6.6+  → setWantsToBeOnActiveScreen (preferred)
#   6.0-6.5 → setScreenConfiguration(ScreenFromCompositor) (deprecated 6.6)
#   < 6.0 → neither API exists (Ubuntu 24.04 ships 5.27.11); main.cpp falls
#           back to QWindow::setScreen(primaryScreen()) — same-monitor pin.
if(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.6)
    target_compile_definitions(schnelle-umlaute-overlay
        PRIVATE SCHNELLE_LAYERSHELLQT_HAS_ACTIVE_SCREEN)
elseif(LayerShellQt_VERSION VERSION_GREATER_EQUAL 6.0)
    # Mutually exclusive with the 6.6 branch: 6.6+ uses the new API and
    # must not advertise the deprecated SCREEN_CONFIG path. Matches the
    # #if/#elif structure in main.cpp 1:1 so exactly one tier is active.
    target_compile_definitions(schnelle-umlaute-overlay
        PRIVATE SCHNELLE_LAYERSHELLQT_HAS_SCREEN_CONFIG)
endif()

install(TARGETS schnelle-umlaute-overlay
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# DBus session services: dbus only scans /usr/share/dbus-1/services by
# default, not /usr/local/share/… Use a hardcoded path so on-demand
# activation works whether the user installs under /usr or /usr/local.
# The Exec= line is templated via configure_file() so it tracks the
# actual install prefix (CMAKE_INSTALL_FULL_BINDIR), avoiding stale
# /usr/local/bin paths when the user builds with -DCMAKE_INSTALL_PREFIX=/usr.
# The daemon is started lazily the first time the addon calls Show() (or
# when the user enables the overlay in the editor); we no longer install
# an XDG autostart entry because that ran before Plasma had configured
# the primary output, pinning the daemon to the login-screen monitor.
configure_file(
    de.schnelle_umlaute.Overlay.service.in
    de.schnelle_umlaute.Overlay.service
    @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/de.schnelle_umlaute.Overlay.service"
        DESTINATION /usr/share/dbus-1/services)
