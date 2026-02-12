add_library(sh3ds_core STATIC Config.cpp)
add_library(SH3DS::Core ALIAS sh3ds_core)

target_include_directories(
  sh3ds_core
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_core
  PUBLIC
    opencv_core
    spdlog::spdlog
    yaml-cpp::yaml-cpp
)

sh3ds_set_warnings(sh3ds_core)
