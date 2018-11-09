project(oi.lib.proto VERSION 0.1 LANGUAGES CXX)
FIND_PACKAGE(Protobuf REQUIRED)

set(CMAKE_CXX_STANDARD 11)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories("include")
include_directories(${Protobuf_INCLUDE_DIRS})

FILE(GLOB OI_PROTO_FILES "src/*.proto")
protobuf_generate_cpp(OI_PROTO_SRC OI_PROTO_HEADER ${OI_PROTO_FILES})

ADD_LIBRARY(oi.proto SHARED ${OI_PROTO_HEADER} ${OI_PROTO_SRC})
target_link_libraries(oi.proto ${PROTOBUF_LIBRARIES})

source_group("Header Files" FILES ${OI_PROTO_HEADER})

set(${PROJECT_NAME}_INCLUDE_DIRS
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}/include
    CACHE INTERNAL "${PROJECT_NAME}: Include Directories" FORCE)

target_include_directories(oi.proto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

install(TARGETS oi.proto EXPORT OIProtoConfig
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT OIProtoConfig DESTINATION share/OIProto/cmake)

# This makes the project importable from the build directory
export(TARGETS oi.proto FILE OIProtoConfig.cmake)
