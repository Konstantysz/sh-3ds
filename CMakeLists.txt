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

include(CMakeParseArguments)

if(MSVC)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

function(sh3ds_configure_visual_studio_target target folder)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Target '${target}' does not exist.")
  endif()

  set(options)
  set(oneValueArgs BASE_DIR)
  set(multiValueArgs)
  cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT ARG_BASE_DIR)
    set(ARG_BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  file(
    GLOB _headers
    CONFIGURE_DEPENDS
    "${ARG_BASE_DIR}/*.h"
    "${ARG_BASE_DIR}/*.hh"
    "${ARG_BASE_DIR}/*.hpp"
    "${ARG_BASE_DIR}/*.hxx"
    "${ARG_BASE_DIR}/*.inl"
    "${ARG_BASE_DIR}/*.inc"
    "${ARG_BASE_DIR}/*.ipp"
    "${ARG_BASE_DIR}/*.tpp"
  )
  file(
    GLOB _resources
    CONFIGURE_DEPENDS
    "${ARG_BASE_DIR}/*.rc"
    "${ARG_BASE_DIR}/*.rc2"
    "${ARG_BASE_DIR}/*.ico"
    "${ARG_BASE_DIR}/*.cur"
    "${ARG_BASE_DIR}/*.bmp"
    "${ARG_BASE_DIR}/*.manifest"
    "${ARG_BASE_DIR}/*.png"
    "${ARG_BASE_DIR}/*.qrc"
  )
  if(_headers OR _resources)
    target_sources(${target} PRIVATE ${_headers} ${_resources})
  endif()

  if(NOT MSVC)
    return()
  endif()

  set_target_properties(${target} PROPERTIES FOLDER "${folder}")

  get_target_property(_target_sources ${target} SOURCES)
  if(NOT _target_sources)
    return()
  endif()

  set(_header_exts ".h" ".hh" ".hpp" ".hxx" ".inl" ".inc" ".ipp" ".tpp")
  set(_resource_exts ".rc" ".rc2" ".ico" ".cur" ".bmp" ".manifest" ".png" ".qrc")
  set(_header_files)
  set(_source_files)
  set(_resource_files)

  foreach(_file IN LISTS _target_sources)
    if(_file MATCHES "^\\$<")
      continue()
    endif()

    if(NOT IS_ABSOLUTE "${_file}")
      set(_file "${CMAKE_CURRENT_SOURCE_DIR}/${_file}")
    endif()

    get_filename_component(_ext "${_file}" EXT)
    string(TOLOWER "${_ext}" _ext)

    if(_ext IN_LIST _header_exts)
      list(APPEND _header_files "${_file}")
    elseif(_ext IN_LIST _resource_exts)
      list(APPEND _resource_files "${_file}")
    else()
      list(APPEND _source_files "${_file}")
    endif()
  endforeach()

  if(_header_files)
    source_group("Header Files" FILES ${_header_files})
  endif()
  if(_source_files)
    source_group("Source Files" FILES ${_source_files})
  endif()
  if(_resource_files)
    source_group("Resource Files" FILES ${_resource_files})
  endif()
endfunction()

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
