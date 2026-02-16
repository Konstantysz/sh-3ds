find_package(CLI11 REQUIRED)

add_executable(sh3ds Sh3DSAppGUI.cpp)
target_link_libraries(sh3ds PRIVATE SH3DS::Pipeline SH3DS::App CLI11::CLI11)

sh3ds_set_warnings(sh3ds)

# Sync config/ directory next to executable (clean first to remove stale files)
add_custom_command(TARGET sh3ds POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:sh3ds>/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:sh3ds>/config"
    COMMENT "Syncing config/ to output directory"
)
