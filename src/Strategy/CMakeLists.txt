add_library(sh3ds_strategy STATIC SoftResetStrategy.cpp)
add_library(SH3DS::Strategy ALIAS sh3ds_strategy)

target_include_directories(
  sh3ds_strategy
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_strategy
  PUBLIC
    SH3DS::Core
    SH3DS::Input
)

sh3ds_set_warnings(sh3ds_strategy)
sh3ds_configure_visual_studio_target(
  sh3ds_strategy
  "Libraries"
  BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)
