CMAKE_MINIMUM_REQUIRED(VERSION 3.2)
PROJECT(oi.native VERSION 0.1 LANGUAGES CXX C)

INCLUDE(GNUInstallDirs)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
#SET(CMAKE_DISABLE_SOURCE_CHANGES  ON)
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(SEND_ERROR "In-source builds are not allowed.")
endif ()
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)

if (WIN32)
  set(CMAKE_SHARED_LIBRARY_PREFIX "")
endif ()

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules" ) 

SET(OI_DEPENDS_DIR "${PROJECT_SOURCE_DIR}/../dependencies" CACHE STRING "OpenIMPRESS dependency directory")
SET(OI_SHARED_DIR  "${PROJECT_SOURCE_DIR}/../shared" CACHE STRING "OpenIMPRESS shared resources")
SET(OI_LIB_DIR  "${PROJECT_SOURCE_DIR}/lib" CACHE STRING "OpenIMPRESS single file library dependencies")

#add_subdirectory(oi.proto)
add_subdirectory(oi.core)
add_subdirectory(oi.network)
add_subdirectory(oi.rgbd)
add_subdirectory(oi.examples)
