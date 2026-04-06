find_package(Fcitx5Module REQUIRED COMPONENTS TestFrontend TestIM)

configure_file(testdir.h.in ${CMAKE_CURRENT_BINARY_DIR}/testdir.h @ONLY)

add_subdirectory(addon)
add_subdirectory(inputmethod)

add_executable(testschnelleumlaute testschnelleumlaute.cpp)
target_include_directories(testschnelleumlaute PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(testschnelleumlaute Fcitx5::Core Fcitx5::Module::TestFrontend Fcitx5::Module::TestIM PkgConfig::XKBCommon)
add_dependencies(testschnelleumlaute copy-addon-conf copy-im-conf schnelle-umlaute)
add_test(NAME testschnelleumlaute COMMAND testschnelleumlaute)
