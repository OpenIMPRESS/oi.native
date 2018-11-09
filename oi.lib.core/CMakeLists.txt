project(oi.lib.core VERSION 0.1 LANGUAGES CXX)

file(GLOB SRC "src/*.cpp")
file(GLOB HEADERS "include/*.hpp" "src/*.hpp")
add_library(oi.core
    ${SRC} ${HEADERS})
source_group("Header Files" FILES ${HEADERS})

target_include_directories(oi.core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(oi.core oi.proto)

# 'make install' to the correct locations (provided by GNUInstallDirs).
install(TARGETS oi.core EXPORT OICoreConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# This makes the project importable from the install directory
# Put config file in per-project dir (name MUST match), can also
# just go into 'cmake'.
install(EXPORT OICoreConfig DESTINATION share/OICore/cmake)

# This makes the project importable from the build directory
export(TARGETS oi.core FILE OICoreConfig.cmake)

# Every library has unit tests, of course
add_executable(oi.core.test
    test/oi.core.test.cpp)

target_link_libraries(oi.core.test
    oi.core)

add_test(oi.core.test oi.core.test)
