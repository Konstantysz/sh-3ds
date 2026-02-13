add_library(
  sh3ds_vision STATIC
  DominantColorDetector.cpp
  HistogramDetector.cpp
  HistogramUtils.cpp
  TemplateMatcher.cpp
)
add_library(SH3DS::Vision ALIAS sh3ds_vision)

target_include_directories(
  sh3ds_vision
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_vision
  PUBLIC
    SH3DS::Core
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    spdlog::spdlog
)

sh3ds_set_warnings(sh3ds_vision)
