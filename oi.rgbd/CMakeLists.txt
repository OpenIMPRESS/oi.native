project(oi.rgbd VERSION 0.1 LANGUAGES CXX)

FIND_PACKAGE(ASIO REQUIRED)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASIO_STANDALONE=YES" )

file(GLOB SRC "src/*.cpp")
file(GLOB HEADERS "include/*.hpp" "src/*.hpp")
add_library(${PROJECT_NAME} ${SRC} ${HEADERS})
source_group("Header Files" FILES ${HEADERS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(${PROJECT_NAME} oi.network)

INCLUDE_DIRECTORIES(${OI_LIB_DIR})

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS ${PROJECT_NAME} EXPORT OIRGBDConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# This makes the project importable from the install directory
# Put config file in per-project dir (name MUST match), can also
# just go into 'cmake'.
install(EXPORT OIRGBDConfig DESTINATION share/OIRGBD/cmake)

# This makes the project importable from the build directory
export(TARGETS ${PROJECT_NAME} FILE OIRGBDConfig.cmake)

# TESTS
add_executable(${PROJECT_NAME}.test
    test/${PROJECT_NAME}.test.cpp
    # ...
)
target_link_libraries(${PROJECT_NAME}.test ${PROJECT_NAME})
add_test(${PROJECT_NAME}.test ${PROJECT_NAME}.test)
