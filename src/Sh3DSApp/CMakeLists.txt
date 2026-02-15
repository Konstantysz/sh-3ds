find_package(CLI11 REQUIRED)

add_executable(sh3ds Sh3DSAppGUI.cpp)
target_link_libraries(sh3ds PRIVATE SH3DS::Pipeline CLI11::CLI11)

sh3ds_set_warnings(sh3ds)
