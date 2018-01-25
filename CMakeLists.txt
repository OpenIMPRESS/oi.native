cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(oi.native VERSION 0.1 LANGUAGES CXX C)

include(GNUInstallDirs)

set (CMAKE_CXX_STANDARD 11)

LIST(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake_modules" ) 

SET(OI_DEPENDS_DIR "${PROJECT_SOURCE_DIR}/../dependencies" CACHE STRING "OpenIMPRESS dependency directory")
SET(OI_LIB_DIR "${PROJECT_SOURCE_DIR}/lib")
#SET(LIBAV_ROOT_DIR "${OI_DEPENDS_DIR}/libav" CACHE STRING "LIBAV directory")
#SET(ASIO_DIR "${OI_DEPENDS_DIR}/asio" CACHE STRING "ASIO directory")

#FIND_PACKAGE(TurboJPEG REQUIRED)
#FIND_PACKAGE(LibAV REQUIRED)

# Targets that we develop here
#enable_testing()
add_subdirectory(oi.lib.core)
add_subdirectory(oi.lib.network)
add_subdirectory(oi.lib.rgbd)
add_subdirectory(oi.basics)
add_subdirectory(oi.tool.rgbdanalysis)
