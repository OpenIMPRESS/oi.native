project(oi.core VERSION 0.1 LANGUAGES CXX)

#FIND_PACKAGE(Protobuf REQUIRED)

file(GLOB SRC "src/*.cpp")
file(GLOB HEADERS "include/*.hpp" "src/*.hpp")
add_library(${PROJECT_NAME} ${SRC} ${HEADERS})
source_group("Header Files" FILES ${HEADERS})

#include_directories(${Protobuf_INCLUDE_DIRS})
#include_directories(${oi.proto_INCLUDE_DIRS})

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

#target_link_libraries(${PROJECT_NAME} oi.proto)


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
export(TARGETS ${PROJECT_NAME} FILE OICoreConfig.cmake)

# TESTS
add_executable(${PROJECT_NAME}.test
    test/${PROJECT_NAME}.test.cpp
)
add_test(${PROJECT_NAME}.test ${PROJECT_NAME}.test)
target_link_libraries(${PROJECT_NAME}.test ${PROJECT_NAME})
add_executable(oi.io.test
    test/oi.io.test.cpp
)
target_link_libraries(oi.io.test ${PROJECT_NAME})
add_test(oi.io.test oi.io.test)
