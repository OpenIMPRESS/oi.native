find_package(OpenCV)
include_directories(${OpenCV_INCLUDE_DIRS})
add_executable(oi.tool.rgbdanalysis src/main.cpp)
target_link_libraries(oi.tool.rgbdanalysis
    ${OpenCV_LIBS} oi.core oi.rgbd)
