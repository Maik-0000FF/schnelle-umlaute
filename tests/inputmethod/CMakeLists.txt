add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/schnelle-umlaute.conf"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${PROJECT_SOURCE_DIR}/data/schnelle-umlaute-inputmethod.conf"
        "${CMAKE_CURRENT_BINARY_DIR}/schnelle-umlaute.conf"
    DEPENDS "${PROJECT_SOURCE_DIR}/data/schnelle-umlaute-inputmethod.conf"
)
add_custom_target(copy-im-conf ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/schnelle-umlaute.conf"
)
