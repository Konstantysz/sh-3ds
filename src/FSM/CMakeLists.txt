add_library(sh3ds_fsm STATIC CXXStateTreeFSM.cpp HuntProfiles.cpp)
add_library(SH3DS::FSM ALIAS sh3ds_fsm)

target_include_directories(
  sh3ds_fsm
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_fsm
  PUBLIC
    SH3DS::Core
    SH3DS::Vision
    CXXStateTree
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
)

sh3ds_set_warnings(sh3ds_fsm)
sh3ds_configure_visual_studio_target(
  sh3ds_fsm
  "Libraries"
  BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
)
