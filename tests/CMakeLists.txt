find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend TestIM)

# Extra Qt components only tests need (DBus adaptor headers in
# OverlayController.h, QSignalSpy for verifying signal emission).
find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS DBus Test)

configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)

add_subdirectory(addon)
add_subdirectory(inputmethod)

add_executable(testschnelleumlaute testschnelleumlaute.cpp)
target_include_directories(testschnelleumlaute PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(testschnelleumlaute Fcitx5::Core Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM PkgConfig::XKBCommon)
add_dependencies(testschnelleumlaute copy-addon-conf copy-im-conf schnelle-umlaute)
add_test(NAME testschnelleumlaute COMMAND testschnelleumlaute)
set_tests_properties(testschnelleumlaute PROPERTIES TIMEOUT 30)

# Standalone unit test for the mappings-io parser — no fcitx5 runtime needed.
add_executable(testmappingsio testmappingsio.cpp)
target_include_directories(testmappingsio PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME testmappingsio COMMAND testmappingsio)
set_tests_properties(testmappingsio PROPERTIES TIMEOUT 10)

# Unit test for MappingModel::isValid{Input,Output}. The validation methods
# are inline in model.h and only depend on QString/QChar, so this test links
# Qt6::Core alone — no Fcitx5 runtime, no MOC, no model.cpp compilation.
add_executable(testmappingmodel testmappingmodel.cpp)
target_include_directories(testmappingmodel PRIVATE
    ${PROJECT_SOURCE_DIR}/config-editor)
target_link_libraries(testmappingmodel Qt${QT_MAJOR_VERSION}::Core)
add_test(NAME testmappingmodel COMMAND testmappingmodel)
set_tests_properties(testmappingmodel PROPERTIES TIMEOUT 10)

# Integration test for MappingModel state + persistence: save/load round-trip,
# load-fallback semantics, and the stateful editor ops (addItem, deleteItem,
# moveUp, moveDown, hasInput, needSave). Links model.cpp → needs AUTOMOC for
# the Q_OBJECT signal and Fcitx5::Utils for StandardPaths/fs/i18n. The test
# redirects XDG_CONFIG_HOME to a tempdir so it never touches ~/.config.
add_executable(testmappingmodelio
    testmappingmodelio.cpp
    ${PROJECT_SOURCE_DIR}/config-editor/model.cpp)
set_target_properties(testmappingmodelio PROPERTIES AUTOMOC ON)
target_include_directories(testmappingmodelio PRIVATE
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/config-editor)
target_link_libraries(testmappingmodelio
    Qt${QT_MAJOR_VERSION}::Core
    Fcitx5::Utils)
add_test(NAME testmappingmodelio COMMAND testmappingmodelio)
set_tests_properties(testmappingmodelio PROPERTIES TIMEOUT 10)

# Unit test for decideOverlayLifecycleAction(). Pure header-only logic —
# no DBus, no Qt, no fcitx5 runtime needed. Guards the rule table that
# drives the daemon start/stop on enable-flag transitions.
add_executable(testoverlaylifecycle testoverlaylifecycle.cpp)
target_include_directories(testoverlaylifecycle PRIVATE ${PROJECT_SOURCE_DIR})
add_test(NAME testoverlaylifecycle COMMAND testoverlaylifecycle)
set_tests_properties(testoverlaylifecycle PROPERTIES TIMEOUT 10)

# Unit test for OverlayController state machine. Compiles OverlayController.cpp
# directly (no LayerShellQt, no QML). Uses QSignalSpy to verify stateChanged
# emissions and a short exec() loop to confirm quit() reaches QCoreApplication.
add_executable(testoverlaycontroller
    testoverlaycontroller.cpp
    ${PROJECT_SOURCE_DIR}/overlay/OverlayController.cpp)
set_target_properties(testoverlaycontroller PROPERTIES AUTOMOC ON)
target_include_directories(testoverlaycontroller PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(testoverlaycontroller
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::DBus
    Qt${QT_MAJOR_VERSION}::Test)
add_test(NAME testoverlaycontroller COMMAND testoverlaycontroller)
set_tests_properties(testoverlaycontroller PROPERTIES TIMEOUT 10)
