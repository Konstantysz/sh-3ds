add_library(sh3ds_pipeline STATIC Orchestrator.cpp)
add_library(SH3DS::Pipeline ALIAS sh3ds_pipeline)

target_include_directories(
  sh3ds_pipeline
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_pipeline
  PUBLIC
    SH3DS::Core
    SH3DS::Capture
    SH3DS::FSM
    SH3DS::Vision
    SH3DS::Strategy
    SH3DS::Input
)

sh3ds_set_warnings(sh3ds_pipeline)
