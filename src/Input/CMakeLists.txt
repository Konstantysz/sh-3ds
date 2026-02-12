add_library(sh3ds_input STATIC MockInputAdapter.cpp)
add_library(SH3DS::Input ALIAS sh3ds_input)

target_include_directories(
  sh3ds_input
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

sh3ds_set_warnings(sh3ds_input)
