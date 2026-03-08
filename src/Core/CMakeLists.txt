add_library(sh3ds_core STATIC Config.cpp)
add_library(SH3DS::Core ALIAS sh3ds_core)

target_include_directories(
  sh3ds_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_core
  PUBLIC
    Kappa
    opencv_core
    yaml-cpp::yaml-cpp
)

sh3ds_set_warnings(sh3ds_core)
sh3ds_configure_visual_studio_target(
  sh3ds_core
  "Libraries"
  BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)
