CMAKE_MINIMUM_REQUIRED(VERSION 3.2)

INCLUDE(GNUInstallDirs)

add_subdirectory(oi.client.rgbd.libfreenect2)

IF (WIN32)
    add_subdirectory(oi.client.rgbd.kinectsdk2)
ENDIF()
