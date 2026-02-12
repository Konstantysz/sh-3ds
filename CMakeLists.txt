cmake_minimum_required(VERSION 3.25)

project(
  sh3ds
  VERSION 0.1.0
  DESCRIPTION "SH-3DS: Networked Shiny Hunting Bot (Vision-driven)"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(CLI11 REQUIRED)

add_subdirectory(src/Core)
add_subdirectory(src/Input)

# TODO: re-enable once namespace migration is complete
# add_subdirectory(src/Capture)
# add_subdirectory(src/FSM)
# add_subdirectory(src/Vision)
# add_subdirectory(src/Strategy)
# add_subdirectory(src/Orchestrator)
#
# add_executable(sh3ds src/Main.cpp)
# target_link_libraries(sh3ds PRIVATE SH3DS::Orchestrator CLI11::CLI11)
# sh3ds_set_warnings(sh3ds)

option(SH3DS_BUILD_TESTS "Build tests" ON)

if(SH3DS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
