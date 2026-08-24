find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend TestIM)

# xkbcommon is a TEST-only dependency. The addon itself no longer compiles a
# keymap (it takes physical keycodes straight from the key events), so only
# testschnelleumlaute needs it: it builds real de/fr/dvorak keymaps to work out
# which physical key produces a given character on each layout, and then checks
# the shipped classifier against those positions.
pkg_check_modules(XKBCommon REQUIRED IMPORTED_TARGET xkbcommon)

# Extra Qt components only tests need (DBus adaptor headers in
# OverlayController.h, QSignalSpy for verifying signal emission).
find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS DBus Test)

configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)

add_subdirectory(addon)
add_subdirectory(inputmethod)

# Shared strict warning flags for all test binaries — same as the
# production library, editor, and overlay targets so test code is
# held to the same bar.
set(SU_TEST_WARN_FLAGS -Wall -Wextra -Wpedantic)

add_executable(testschnelleumlaute testschnelleumlaute.cpp)
target_include_directories(testschnelleumlaute PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${PROJECT_SOURCE_DIR})
target_compile_options(testschnelleumlaute PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testschnelleumlaute Fcitx5::Core Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM PkgConfig::XKBCommon)
add_dependencies(testschnelleumlaute copy-addon-conf copy-im-conf schnelle-umlaute)
add_test(NAME testschnelleumlaute COMMAND testschnelleumlaute)
# The cycling-watchdog tests wait real timers out, and the ceiling test alone
# runs for ten backstops. Generous, so a loaded CI machine cannot turn a slow
# run into a failure.
set_tests_properties(testschnelleumlaute PROPERTIES TIMEOUT 60)

# Unit test for readLine() — the truncation-guarded line reader every config
# parser shares. Header-only, no fcitx5 runtime needed.
add_executable(testlineio testlineio.cpp)
target_include_directories(testlineio PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testlineio PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testlineio COMMAND testlineio)
set_tests_properties(testlineio PROPERTIES TIMEOUT 10)

# Standalone unit test for the mappings-io parser — no fcitx5 runtime needed.
add_executable(testmappingsio testmappingsio.cpp)
target_include_directories(testmappingsio PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testmappingsio PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testmappingsio COMMAND testmappingsio)
set_tests_properties(testmappingsio PROPERTIES TIMEOUT 10)

# Unit test for decideOverlayLifecycleAction(). Pure header-only logic —
# no DBus, no Qt, no fcitx5 runtime needed. Guards the rule table that
# drives the daemon start/stop on enable-flag transitions.
add_executable(testoverlaylifecycle testoverlaylifecycle.cpp)
target_include_directories(testoverlaylifecycle PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testoverlaylifecycle PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testoverlaylifecycle COMMAND testoverlaylifecycle)
set_tests_properties(testoverlaylifecycle PROPERTIES TIMEOUT 10)

# Unit test for checkLayerShellCapability(). Pure header-only logic — guards
# the session detection that decides whether the overlay can run on the
# user's compositor (GNOME/X11 → no, KDE/sway/Hyprland → yes).
add_executable(testlayershellcapability testlayershellcapability.cpp)
target_include_directories(testlayershellcapability PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testlayershellcapability PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testlayershellcapability COMMAND testlayershellcapability)
set_tests_properties(testlayershellcapability PROPERTIES TIMEOUT 10)

# Unit test for isSyntheticAutoRepeatRelease(). Pure header-only logic that
# guards the predicate that recognises a held key's synthetic auto-repeat
# release by its frozen frontend timestamp (issue #73).
add_executable(testsyntheticautorepeat testsyntheticautorepeat.cpp)
target_include_directories(testsyntheticautorepeat PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testsyntheticautorepeat PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testsyntheticautorepeat COMMAND testsyntheticautorepeat)
set_tests_properties(testsyntheticautorepeat PROPERTIES TIMEOUT 10)

# Unit test for decideRenderAction(). Pure header-only logic that guards the
# renderer's show/hide/no-op table, above all the no-op that keeps cycling from
# rebuilding the window on every keystroke.
add_executable(testoverlayrender testoverlayrender.cpp)
target_include_directories(testoverlayrender PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testoverlayrender PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testoverlayrender COMMAND testoverlayrender)
set_tests_properties(testoverlayrender PROPERTIES TIMEOUT 10)

# Unit test for the KWin cursor source's query-id rules. Pure header-only logic
# that guards which SendCursor reply the daemon may act on, so a reply that
# outlived its own query cannot move the overlay that is open now.
add_executable(testcursorrequest testcursorrequest.cpp)
target_include_directories(testcursorrequest PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testcursorrequest PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testcursorrequest COMMAND testcursorrequest)
set_tests_properties(testcursorrequest PROPERTIES TIMEOUT 10)

# Unit test for the cursor-overlay geometry helpers. Pure header-only logic —
# guards the "Cursor:" wire-format split and the lower-left-at-cursor margin
# math (clamping to the cursor's output) that the overlay daemon relies on.
add_executable(testcursorgeometry testcursorgeometry.cpp)
target_include_directories(testcursorgeometry PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testcursorgeometry PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testcursorgeometry COMMAND testcursorgeometry)
set_tests_properties(testcursorgeometry PROPERTIES TIMEOUT 10)

# Unit test for the progress-bar geometry helpers. Pure header-only logic —
# guards the bar length clamp/scale, the lead/window split, and the grid
# placement that centres the panel (not the panel+bar surface) on a column,
# shared by the overlay daemon and QML.
add_executable(testprogressgeometry testprogressgeometry.cpp)
target_include_directories(testprogressgeometry PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testprogressgeometry PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testprogressgeometry COMMAND testprogressgeometry)
set_tests_properties(testprogressgeometry PROPERTIES TIMEOUT 10)

# Unit test for classifySessionEnv(). Pure header-only logic — guards the
# rule table that decides between the "logout pending" and "compositor
# config" setup dialogs (environment.d sessions vs TTY-launched
# compositors like Hyprland).
add_executable(testsessionenv testsessionenv.cpp)
target_include_directories(testsessionenv PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testsessionenv PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testsessionenv COMMAND testsessionenv)
set_tests_properties(testsessionenv PROPERTIES TIMEOUT 10)

# Unit test for EnvSetup::isConfigured(), the Plasma-aware environment check.
# Compiles EnvSetup.cpp directly (Qt Core only, no DBus/QML) and drives the
# desktop matrix through setenv(); isConfigured() reads only the environment.
add_executable(testenvsetup
    testenvsetup.cpp
    ${PROJECT_SOURCE_DIR}/editor/EnvSetup.cpp)
set_target_properties(testenvsetup PROPERTIES AUTOMOC ON)
target_include_directories(testenvsetup PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testenvsetup PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testenvsetup Qt${QT_MAJOR_VERSION}::Core)
add_test(NAME testenvsetup COMMAND testenvsetup)
set_tests_properties(testenvsetup PROPERTIES TIMEOUT 10)

# Unit test for OverlayController state machine. Compiles OverlayController.cpp
# directly (no LayerShellQt, no QML). Uses QSignalSpy to verify stateChanged
# emissions and a short exec() loop to confirm quit() reaches QCoreApplication.
add_executable(testoverlaycontroller
    testoverlaycontroller.cpp
    ${PROJECT_SOURCE_DIR}/overlay/OverlayController.cpp)
set_target_properties(testoverlaycontroller PROPERTIES AUTOMOC ON)
target_include_directories(testoverlaycontroller PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testoverlaycontroller PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testoverlaycontroller PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testoverlaycontroller
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME testoverlaycontroller COMMAND testoverlaycontroller)
set_tests_properties(testoverlaycontroller PROPERTIES TIMEOUT 10)

# Unit test for OverlayController theme state. Verifies the validator and
# the themeChanged signal the overlay QML bindings depend on.
add_executable(testoverlaytheme
    testoverlaytheme.cpp
    ${PROJECT_SOURCE_DIR}/overlay/OverlayController.cpp)
set_target_properties(testoverlaytheme PROPERTIES AUTOMOC ON)
target_include_directories(testoverlaytheme PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testoverlaytheme PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testoverlaytheme PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testoverlaytheme
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME testoverlaytheme COMMAND testoverlaytheme)
set_tests_properties(testoverlaytheme PROPERTIES TIMEOUT 10)

# Standalone editor — validation layer of MappingListModel. Links
# MappingListModel.cpp so every test case runs against the same code the
# QML editor uses; AUTOMOC handles the Q_OBJECT signals. Redirects
# XDG_CONFIG_HOME at runtime so the constructor's implicit load() never
# touches ~/.config.
add_executable(testmappinglistmodel
    testmappinglistmodel.cpp
    ${PROJECT_SOURCE_DIR}/editor/MappingListModel.cpp)
set_target_properties(testmappinglistmodel PROPERTIES AUTOMOC ON)
target_include_directories(testmappinglistmodel PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testmappinglistmodel PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testmappinglistmodel PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testmappinglistmodel
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME testmappinglistmodel COMMAND testmappinglistmodel)
set_tests_properties(testmappinglistmodel PROPERTIES TIMEOUT 10)

# Standalone editor — state + persistence for MappingListModel. Covers
# add/remove/update/move, the defaults-on-empty-file fallback, reload(),
# and the mappings.txt on-disk format.
add_executable(testmappinglistmodelio
    testmappinglistmodelio.cpp
    ${PROJECT_SOURCE_DIR}/editor/MappingListModel.cpp)
set_target_properties(testmappinglistmodelio PROPERTIES AUTOMOC ON)
target_include_directories(testmappinglistmodelio PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testmappinglistmodelio PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testmappinglistmodelio PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testmappinglistmodelio
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME testmappinglistmodelio COMMAND testmappinglistmodelio)
set_tests_properties(testmappinglistmodelio PROPERTIES TIMEOUT 10)

# Standalone editor — MappingListModel's composed (merge) view. Seeds profile
# files + merge.conf/usage.conf in a scratch XDG_CONFIG_HOME and drives the
# base-active model through composing gating, deterministic row order, chip
# provenance, cascade delete, cross-row move, order override, duplicate
# detection, and the usage-frequency preview.
add_executable(testmappinglistmodelmerge
    testmappinglistmodelmerge.cpp
    ${PROJECT_SOURCE_DIR}/editor/MappingListModel.cpp)
set_target_properties(testmappinglistmodelmerge PROPERTIES AUTOMOC ON)
target_include_directories(testmappinglistmodelmerge PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testmappinglistmodelmerge PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testmappinglistmodelmerge PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testmappinglistmodelmerge
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME testmappinglistmodelmerge COMMAND testmappinglistmodelmerge)
set_tests_properties(testmappinglistmodelmerge PROPERTIES TIMEOUT 10)

# Standalone editor — SettingsModel validators, legacy position migration,
# app-filter list dedup, and the save/load round-trip against
# schnelle-umlaute.conf. SettingsModel depends on OverlayDBusClient for
# sendTheme, so it has to be linked too; the actual DBus call is a no-op
# without a session bus, which keeps the test hermetic.
add_executable(testsettingsmodel
    testsettingsmodel.cpp
    ${PROJECT_SOURCE_DIR}/editor/SettingsModel.cpp
    ${PROJECT_SOURCE_DIR}/editor/OverlayDBusClient.cpp)
set_target_properties(testsettingsmodel PROPERTIES AUTOMOC ON)
target_include_directories(testsettingsmodel PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testsettingsmodel PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testsettingsmodel PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testsettingsmodel
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    # SettingsModel reads QGuiApplication::styleHints() to derive the theme from
    # the desktop's light/dark setting. The test runs headless, where styleHints
    # is null and the derivation falls back, but the header still has to resolve.
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME testsettingsmodel COMMAND testsettingsmodel)
# Longer than the other unit tests: testSaveErrorRepeatsAfterWindow waits out
# SettingsModel::kSaveErrorRepeatMs (2s) for real, so 10s left too little room
# on a loaded runner.
set_tests_properties(testsettingsmodel PROPERTIES TIMEOUT 30)

# Pure profile-cycle logic (profile_cycle.h): favorites-vs-all selection and
# the wrapping next/previous step. No fcitx/Qt runtime needed.
add_executable(testprofilecycle testprofilecycle.cpp)
target_include_directories(testprofilecycle PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testprofilecycle PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testprofilecycle COMMAND testprofilecycle)
set_tests_properties(testprofilecycle PROPERTIES TIMEOUT 10)

# Engine-side loader: per-profile defaults rule (only the Standard profile
# seeds the German defaults; other profiles load empty). Links mappings_loader
# + Fcitx5::Core (StandardPaths); no Qt.
add_executable(testprofilesloader
    testprofilesloader.cpp
    ${PROJECT_SOURCE_DIR}/src/mappings_loader.cpp)
target_include_directories(testprofilesloader PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src)
target_compile_options(testprofilesloader PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testprofilesloader Fcitx5::Core)
add_test(NAME testprofilesloader COMMAND testprofilesloader)
set_tests_properties(testprofilesloader PROPERTIES TIMEOUT 10)

# Engine-side usage-reset helpers (mappings_loader): the one-shot marker consume
# (takeUsageResetMarker) and usage.conf delete (deleteUsage), the core of the
# reset handshake. Same harness as testprofilesloader (mappings_loader +
# Fcitx5::Core, XDG_CONFIG_HOME redirected to a scratch dir).
add_executable(testusagereset
    testusagereset.cpp
    ${PROJECT_SOURCE_DIR}/src/mappings_loader.cpp)
target_include_directories(testusagereset PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src)
target_compile_options(testusagereset PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testusagereset Fcitx5::Core)
add_test(NAME testusagereset COMMAND testusagereset)
set_tests_properties(testusagereset PROPERTIES TIMEOUT 10)

# Editor Qt-to-fcitx combo conversion round-trip: the captured-combo strings
# must parse with fcitx Key() and match a real press after normalization.
# Links keycombo.cpp (pure, no QML) + Fcitx5::Core (Key) + Qt Core (QString).
add_executable(testkeycombo
    testkeycombo.cpp
    ${PROJECT_SOURCE_DIR}/editor/keycombo.cpp)
target_include_directories(testkeycombo PRIVATE ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testkeycombo PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testkeycombo PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testkeycombo Qt${QT_MAJOR_VERSION}::Core Fcitx5::Core)
add_test(NAME testkeycombo COMMAND testkeycombo)
set_tests_properties(testkeycombo PROPERTIES TIMEOUT 10)

# Standalone editor: ProfileListModel CRUD, name + slug uniqueness,
# Standard/last-profile protection, active reassignment on delete, first-run
# seeding, and the editor's own profiles.conf write/read round-trip. Same
# XDG_CONFIG_HOME redirect + DBus-no-op pattern as testmappinglistmodel.
add_executable(testprofilelistmodel
    testprofilelistmodel.cpp
    ${PROJECT_SOURCE_DIR}/editor/ProfileListModel.cpp)
set_target_properties(testprofilelistmodel PROPERTIES AUTOMOC ON)
target_include_directories(testprofilelistmodel PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testprofilelistmodel PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testprofilelistmodel PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testprofilelistmodel
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Qml
    Fcitx5::Core)
add_test(NAME testprofilelistmodel COMMAND testprofilelistmodel)
set_tests_properties(testprofilelistmodel PROPERTIES TIMEOUT 10)

# Cross-component round-trip: the editor's ProfileListModel writer and the
# engine's ProfilesConfig (config.h) reader must agree on the profiles.conf
# INI format. Links both ProfileListModel.cpp (Qt) and Fcitx5::Core (readAsIni
# / safeSaveAsIni); QT_NO_KEYWORDS keeps the fcitx headers safe under Qt.
add_executable(testprofilesconf
    testprofilesconf.cpp
    ${PROJECT_SOURCE_DIR}/editor/ProfileListModel.cpp)
set_target_properties(testprofilesconf PROPERTIES AUTOMOC ON)
target_include_directories(testprofilesconf PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/editor)
target_compile_options(testprofilesconf PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testprofilesconf PRIVATE QT_NO_KEYWORDS)
target_link_libraries(testprofilesconf
    Fcitx5::Core
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME testprofilesconf COMMAND testprofilesconf)
set_tests_properties(testprofilesconf PROPERTIES TIMEOUT 10)

# Unit test for the pure caret-theme helpers (caret_theme.h): colour
# normalisation, the classicui.conf parse/patch round-trip, and the generated
# theme.conf colour-role mapping. Pure QString logic — only Qt6::Core, no
# SettingsModel/DBus/QML link needed.
add_executable(testcarettheme testcarettheme.cpp)
target_include_directories(testcarettheme PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testcarettheme PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testcarettheme Qt${QT_MAJOR_VERSION}::Core)
add_test(NAME testcarettheme COMMAND testcarettheme)
set_tests_properties(testcarettheme PROPERTIES TIMEOUT 10)

# Unit test for the file side (caret_theme_io.h): what restore() does to the
# user's own classicui theme, including the case where the backup exists but
# yields nothing. Runs against a temporary XDG_CONFIG_HOME, and needs DBus on
# the link line because the header ends its writes with a classicui reload.
add_executable(testcaretthemeio testcaretthemeio.cpp)
target_include_directories(testcaretthemeio PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testcaretthemeio PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testcaretthemeio Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::DBus)
add_test(NAME testcaretthemeio COMMAND testcaretthemeio)
set_tests_properties(testcaretthemeio PROPERTIES TIMEOUT 10)

# Pure base-anchored merge compose (profile_compose.h): duplicate-preserving
# composition with per-instance provenance, self-healing order overrides, and
# the runtime unique-value projection. No fcitx/Qt runtime needed.
add_executable(testprofilecompose testprofilecompose.cpp)
target_include_directories(testprofilecompose PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testprofilecompose PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testprofilecompose COMMAND testprofilecompose)
set_tests_properties(testprofilecompose PROPERTIES TIMEOUT 10)

# Pure merge-manifest IO (merge_manifest_io.h): the tab-separated base +
# ordered sources + per-base order-override round-trip, shared by engine and
# editor. No fcitx/Qt runtime needed.
add_executable(testmergemanifestio testmergemanifestio.cpp)
target_include_directories(testmergemanifestio PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testmergemanifestio PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testmergemanifestio COMMAND testmergemanifestio)
set_tests_properties(testmergemanifestio PROPERTIES TIMEOUT 10)

# Pure usage-counter IO (usage_io.h): the engine-written / editor-read counter
# file round-trip and malformed-line tolerance. No fcitx/Qt runtime needed.
add_executable(testusageio testusageio.cpp)
target_include_directories(testusageio PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testusageio PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testusageio COMMAND testusageio)
set_tests_properties(testusageio PROPERTIES TIMEOUT 10)

# Pure frequency-sort comparator (usage_sort.h): descending by count, stable
# tie-break on stored order, no-op on all-zero stats. The single comparator the
# engine and the editor both call. No fcitx/Qt runtime needed.
add_executable(testusagesort testusagesort.cpp)
target_include_directories(testusagesort PRIVATE ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testusagesort PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testusagesort COMMAND testusagesort)
set_tests_properties(testusagesort PROPERTIES TIMEOUT 10)

# Guards the C++/QML theme-name sync: the valid names must be identical across
# addon/themes.h (the setTheme/SetTheme guards) and addon/palette/Palettes.qml
# (all/ids/labels). Pure text parsing of the two sources (paths injected below),
# no Qt/QML runtime, so a theme added to only one place fails CI.
add_executable(testthemesync testthemesync.cpp)
target_include_directories(testthemesync PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(testthemesync PRIVATE ${SU_TEST_WARN_FLAGS})
target_compile_definitions(testthemesync PRIVATE
    THEMES_H_PATH="${PROJECT_SOURCE_DIR}/themes.h"
    PALETTES_QML_PATH="${PROJECT_SOURCE_DIR}/palette/Palettes.qml")
add_test(NAME testthemesync COMMAND testthemesync)
set_tests_properties(testthemesync PROPERTIES TIMEOUT 10)
