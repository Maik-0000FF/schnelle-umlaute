find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend TestIM)

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
target_include_directories(testschnelleumlaute PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(testschnelleumlaute PRIVATE ${SU_TEST_WARN_FLAGS})
target_link_libraries(testschnelleumlaute Fcitx5::Core Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM PkgConfig::XKBCommon)
add_dependencies(testschnelleumlaute copy-addon-conf copy-im-conf schnelle-umlaute)
add_test(NAME testschnelleumlaute COMMAND testschnelleumlaute)
set_tests_properties(testschnelleumlaute PROPERTIES TIMEOUT 30)

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

# Unit test for classifySessionEnv(). Pure header-only logic — guards the
# rule table that decides between the "logout pending" and "compositor
# config" setup dialogs (environment.d sessions vs TTY-launched
# compositors like Hyprland).
add_executable(testsessionenv testsessionenv.cpp)
target_include_directories(testsessionenv PRIVATE ${PROJECT_SOURCE_DIR})
target_compile_options(testsessionenv PRIVATE ${SU_TEST_WARN_FLAGS})
add_test(NAME testsessionenv COMMAND testsessionenv)
set_tests_properties(testsessionenv PROPERTIES TIMEOUT 10)

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
    Qt${QT_MAJOR_VERSION}::Qml)
add_test(NAME testsettingsmodel COMMAND testsettingsmodel)
set_tests_properties(testsettingsmodel PROPERTIES TIMEOUT 10)
