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

# Include project modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)
include(Dependencies)

add_subdirectory(src)

option(SH3DS_BUILD_TESTS "Build tests" ON)

if(SH3DS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "=========================================================")
message(STATUS "  SH-3DS: Networked Shiny Hunting Bot v${PROJECT_VERSION}")
message(STATUS "=========================================================")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "=========================================================")
message(STATUS "")