# Invoked by add_custom_command to regenerate git_version.h during incremental builds.
cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_SOURCE_DIR)
    message(FATAL_ERROR "CMAKE_SOURCE_DIR is required")
endif()
if(NOT DEFINED OUTPUT_HEADER)
    message(FATAL_ERROR "OUTPUT_HEADER is required")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/git_version.cmake)
nisense_configure_git_version("${OUTPUT_HEADER}")
