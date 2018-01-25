project(oi.lib.rgbd VERSION 0.1 LANGUAGES CXX)

file(GLOB SRC "src/*.cpp")
file(GLOB HEADERS "include/*.hpp" "src/*.hpp")
add_library(oi.rgbd
    ${SRC} ${HEADERS})
source_group("Header Files" FILES ${HEADERS})

target_include_directories(oi.rgbd PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(oi.rgbd
    oi.network
)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS oi.rgbd EXPORT OIRGBDConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# This makes the project importable from the install directory
# Put config file in per-project dir (name MUST match), can also
# just go into 'cmake'.
install(EXPORT OIRGBDConfig DESTINATION share/OIRGBD/cmake)

# This makes the project importable from the build directory
export(TARGETS oi.rgbd FILE OIRGBDConfig.cmake)

# Every library has unit tests, of course
add_executable(oi.rgbd.test
    test/oi.rgbd.test.cpp)

target_link_libraries(oi.rgbd.test
    oi.rgbd)

add_test(oi.rgbd.test oi.rgbd.test)
