add_library(sh3ds_capture STATIC FileFrameSource.cpp FramePreprocessor.cpp ScreenDetector.cpp VideoFrameSource.cpp)
add_library(SH3DS::Capture ALIAS sh3ds_capture)

target_include_directories(
  sh3ds_capture
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

target_link_libraries(
  sh3ds_capture
  PUBLIC
    SH3DS::Core
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    opencv_videoio
)

sh3ds_set_warnings(sh3ds_capture)
