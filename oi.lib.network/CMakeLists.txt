project(oi.lib.network VERSION 0.1 LANGUAGES CXX)

FIND_PACKAGE(ASIO REQUIRED)
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DASIO_STANDALONE=YES" )

file(GLOB SRC "src/*.cpp")
file(GLOB HEADERS "include/*.hpp" "src/*.hpp")
add_library(oi.network
    ${SRC} ${HEADERS})
source_group("Header Files" FILES ${HEADERS})

target_include_directories(oi.network PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    PRIVATE src)

target_link_libraries(oi.network
    oi.core
)

INCLUDE_DIRECTORIES(${OI_LIB_DIR})
INCLUDE_DIRECTORIES(${ASIO_INCLUDE_DIRS})

install(TARGETS oi.network EXPORT OINetworkConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT OINetworkConfig DESTINATION share/OINetwork/cmake)

export(TARGETS oi.network FILE OINetworkConfig.cmake)

add_executable(oi.network.test
    test/oi.network.test.cpp)

target_link_libraries(oi.network.test
    oi.network)

add_test(oi.network.test oi.network.test)
