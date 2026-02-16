find_package(imgui CONFIG REQUIRED)

add_library(sh3ds_app STATIC
    TextureUploader.cpp
    PlaybackController.cpp
    DebugLayer.cpp
    SH3DSDebugApp.cpp
)
add_library(SH3DS::App ALIAS sh3ds_app)

target_include_directories(
  sh3ds_app
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_app
  PUBLIC
    SH3DS::Core
    SH3DS::Capture
    SH3DS::FSM
    SH3DS::Vision
    Kappa
    imgui::imgui
)

sh3ds_set_warnings(sh3ds_app)
