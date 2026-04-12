find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend TestIM)

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
